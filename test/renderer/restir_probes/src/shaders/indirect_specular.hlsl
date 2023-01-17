#include "utils/pcg32.hlsli"
#include "brdf/trowbridge_reitz.hlsli"
#include "gbuffer_common.hlsli"

#include "shading.hlsli"

ConstantBuffer<probe_constants>               probe_consts     : register(b0, space0);
ConstantBuffer<indirect_specular_constants>   constants        : register(b1, space0);
ConstantBuffer<lighting_constants>            lighting_consts  : register(b2, space0);
StructuredBuffer<direct_lighting_reservoir>   direct_probes    : register(t3, space0);
StructuredBuffer<indirect_lighting_reservoir> indirect_probes  : register(t4, space0);
Texture3D<float4>                             indirect_sh0     : register(t5, space0);
Texture3D<float4>                             indirect_sh1     : register(t6, space0);
Texture3D<float4>                             indirect_sh2     : register(t7, space0);
Texture3D<float4>                             indirect_sh3     : register(t8, space0);
Texture2D<float3>                             diffuse_lighting : register(t9, space0);
Texture2D<float2>                             envmap_lut       : register(t10, space0);
RWTexture2D<float4>                           out_specular     : register(u11, space0);
RaytracingAccelerationStructure               rtas             : register(t12, space0);

Texture2D<float4> gbuffer_albedo_glossiness : register(t13, space0);
Texture2D<float4> gbuffer_normal            : register(t14, space0);
Texture2D<float4> gbuffer_metalness         : register(t15, space0);
Texture2D<float>  gbuffer_depth             : register(t16, space0);

Texture2D<float3> sky_latlong : register(t17, space0);

Texture2D<float4>        textures[]  : register(t0, space1);
StructuredBuffer<float3> positions[] : register(t0, space2);
StructuredBuffer<float3> normals[]   : register(t0, space3);
StructuredBuffer<float4> tangents[]  : register(t0, space4);
StructuredBuffer<float2> uvs[]       : register(t0, space5);
StructuredBuffer<uint>   indices[]   : register(t0, space6);

StructuredBuffer<rt_instance_data>               instances  : register(t0, space7);
StructuredBuffer<geometry_data>                  geometries : register(t1, space7);
StructuredBuffer<generic_pbr_material::material> materials  : register(t2, space7);
StructuredBuffer<light>                          all_lights : register(t3, space7);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space8);

[numthreads(8, 8, 1)]
void main_cs(uint2 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= lighting_consts.screen_size)) {
		return;
	}

	float2 uv = (dispatch_thread_id + 0.5f) / (float2)lighting_consts.screen_size;
	g_buffer::contents gbuf = g_buffer::decode(
		gbuffer_albedo_glossiness[dispatch_thread_id],
		gbuffer_normal           [dispatch_thread_id],
		gbuffer_metalness        [dispatch_thread_id],
		gbuffer_depth            [dispatch_thread_id],
		uv, lighting_consts.inverse_jittered_projection_view, lighting_consts.depth_linearization_constants
	);
	float3 view_vec = normalize(lighting_consts.camera.xyz - gbuf.fragment.position_ws);
	float alpha = max(0.01f, squared(1.0f - gbuf.fragment.glossiness));

	if (gbuf.raw_depth == 0.0f) {
		out_specular[dispatch_thread_id] = (float4)0.0f;
		return;
	}

	if (constants.use_approx_for_everything) {
		sh::sh2 shr, shg, shb;
		fetch_indirect_sh(
			gbuf.fragment, indirect_sh0, indirect_sh1, indirect_sh2, indirect_sh3,
			linear_clamp_sampler, probe_consts, shr, shg, shb
		);
		float3 irr = approximate_indirect_specular(
			gbuf.fragment, view_vec, shr, shg, shb,
			envmap_lut, linear_sampler, lighting_consts.envmaplut_uvscale, lighting_consts.envmaplut_uvbias
		);
		out_specular[dispatch_thread_id] = float4(irr, 0.0f);
		return;
	}

	pcg32::state rng = pcg32::seed(dispatch_thread_id.y * 5003 + dispatch_thread_id.x * 3, constants.frame_index);

	float3 out_dir;
	float canonical_pdf;
	float3 h;
	{
		tangent_frame::tbn ts = tangent_frame::any(gbuf.fragment.normal_ws);
#ifdef SAMPLE_VISIBLE_NORMALS
		float3 projected = float3(dot(view_vec, ts.tangent), dot(view_vec, ts.bitangent), dot(view_vec, ts.normal));
		float3 h_projected = trowbridge_reitz::importance_sample_d_visible(float2(pcg32::random_01(rng), pcg32::random_01(rng)), projected, alpha);
		h = h_projected.x * ts.tangent + h_projected.y * ts.bitangent + h_projected.z * ts.normal;
		canonical_pdf = trowbridge_reitz::importance_sample_d_visible_pdf(dot(ts.normal, view_vec), h_projected.z, saturate(dot(view_vec, h)), alpha);
#else
		float2 smp = trowbridge_reitz::importance_sample_d(pcg32::random_01(rng), alpha);
		float theta = pcg32::random_01(rng) * 2.0f * pi;
		h = ts.normal * smp.x + sqrt(1.0f - smp.x * smp.x) * (ts.tangent * cos(theta) + ts.bitangent * sin(theta));
		canonical_pdf = smp.y;
#endif
	}
	out_dir = reflect(-view_vec, h);
	canonical_pdf /= 4.0f * dot(h, view_vec);

	float3 irradiance = (float3)0.0f;
	{
		RayDesc ray;
		ray.Origin    = gbuf.fragment.position_ws;
		ray.TMin      = 0.001f;
		ray.Direction = out_dir;
		ray.TMax      = max_float_v;

		RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
		ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
		ray_query.Proceed();

		if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) { // shade hit point
			// fetch geometry data
			uint   instance_index = ray_query.CommittedInstanceID();
			float3 hit_position   = ray_query.WorldRayOrigin() + ray_query.CommittedRayT() * ray_query.WorldRayDirection();

			rt_instance_data               inst = instances[instance_index];
			geometry_data                  geom = geometries[inst.geometry_index];
			generic_pbr_material::material mat  = materials[inst.material_index];

			material::shading_properties frag = fetch_material_properties(
				hit_position,
				ray_query.CommittedPrimitiveIndex(),
				ray_query.CommittedTriangleBarycentrics(),
				ray_query.CommittedObjectToWorld3x4(),
				inst, geom, mat,
				textures, positions, normals, tangents, uvs, indices,
				linear_sampler
			);

			// direct
			float3 diffuse_irr;
			float3 specular_irr;
			evaluate_reservoir_direct_lighting(
				frag, -out_dir, direct_probes, all_lights, probe_consts, rng,
				diffuse_irr, specular_irr
			);

			sh::sh2 shr, shg, shb;
			fetch_indirect_sh(
				frag, indirect_sh0, indirect_sh1, indirect_sh2, indirect_sh3,
				linear_clamp_sampler, probe_consts, shr, shg, shb
			);

			// indirect diffuse
			diffuse_irr += evaluate_indirect_diffuse(frag, shr, shg, shb);

			if (constants.approx_indirect_indirect_specular) { // indirect specular
				specular_irr += approximate_indirect_specular(
					frag, -out_dir, shr, shg, shb,
					envmap_lut, linear_sampler, lighting_consts.envmaplut_uvscale, lighting_consts.envmaplut_uvbias
				);
			}

			if (constants.use_screenspace_samples) {
				float4 ss_pos = mul(lighting_consts.jittered_projection_view, float4(hit_position, 1.0f));
				ss_pos.xyz /= ss_pos.w;
				if (all(ss_pos.xy > -1.0f) && all(ss_pos.xy < 1.0f) && ss_pos.z > 0.0f && ss_pos.z < 1.0f) {
					ss_pos.xy = float2(ss_pos.x, -ss_pos.y) * 0.5f + 0.5f;
					float screen_depth = gbuffer_depth.SampleLevel(linear_sampler, ss_pos.xy, 0.0f);
					if (abs(screen_depth - ss_pos.z) < 0.01f) {
						diffuse_irr = diffuse_lighting.SampleLevel(linear_sampler, ss_pos.xy, 0.0f);
					}
				}
			}

			irradiance = diffuse_irr + specular_irr;
		} else {
			irradiance = fetch_sky_latlong(sky_latlong, linear_sampler, out_dir) * lighting_consts.sky_scale;
		}
	}

	float3 brdf_d, brdf_s;
	brdf(gbuf.fragment, out_dir, view_vec, brdf_d, brdf_s);
	float3 lighting = saturate(dot(gbuf.fragment.normal_ws, out_dir)) * brdf_s * irradiance / canonical_pdf;

	if (constants.enable_mis) {
		// probe information
		float3 cell_f = probes::coord_from_position(gbuf.fragment.position_ws, probe_consts);
		uint3 cell_index = (uint3)clamp((int3)cell_f, 0, (int3)probe_consts.grid_size - 2);
		uint3 use_probe = probes::get_random_coord(gbuf.fragment.position_ws, gbuf.fragment.normal_ws, cell_index, probe_consts, pcg32::random_01(rng));
		float3 probe_pos = probes::coord_to_position(use_probe, probe_consts);
		uint use_probe_index = probes::coord_to_index(use_probe, probe_consts);
		uint indirect_reservoir_index = use_probe_index * probe_consts.indirect_reservoirs_per_probe;

		float w = 1.0f;
		float3 res_lighting = (float3)0.0f;
		for (uint i = 0; i < probe_consts.indirect_reservoirs_per_probe; ++i) {
			indirect_lighting_reservoir reservoir = indirect_probes[indirect_reservoir_index + i];
			float3 reservoir_dir;
			if (reservoir.distance >= max_float_v) {
				reservoir_dir = octahedral_mapping::to_direction_normalized(reservoir.direction_octahedral);
			} else {
				float3 reservoir_pos = probe_pos + octahedral_mapping::to_direction_normalized(reservoir.direction_octahedral) * reservoir.distance;
				reservoir_dir = normalize(reservoir_pos - gbuf.fragment.position_ws);
			}
			float3 reservoir_h = normalize(view_vec + reservoir_dir);
			float reservoir_n_h = saturate(dot(reservoir_h, gbuf.fragment.normal_ws));
#ifdef SAMPLE_VISIBLE_NORMALS
			float reservoir_pdf_d = trowbridge_reitz::d(reservoir_n_h, alpha);
#else
			float reservoir_pdf_d = trowbridge_reitz::importance_sample_d_visible_pdf(
				dot(gbuf.fragment.normal_ws, view_vec),
				reservoir_n_h,
				dot(reservoir_h, view_vec),
				alpha
			);
#endif
			reservoir_pdf_d /= 4.0f * dot(reservoir_h, view_vec);
			float reservoir_pdf_l = rcp(max(0.01f, reservoir.data.contribution_weight));
			float3 reservoir_irr = reservoir.irradiance;

			float direct_pdf =
				reservoir_pdf_l *
				max(irradiance.r, max(irradiance.g, irradiance.b)) /
				max(0.01f, max(reservoir_irr.r, max(reservoir_irr.g, reservoir_irr.b)));


			float w1 = canonical_pdf / (canonical_pdf + direct_pdf);
			float w2 = reservoir_pdf_l / (reservoir_pdf_l + reservoir_pdf_d);

			float3 brdf_d2, brdf_s2;
			brdf(gbuf.fragment, reservoir_dir, view_vec, brdf_d2, brdf_s2);

			w += w1;
			res_lighting += w2 * saturate(dot(gbuf.fragment.normal_ws, reservoir_dir)) * brdf_s2 * reservoir_irr * reservoir.data.contribution_weight;
		}

		lighting = (lighting * w + res_lighting) / (probe_consts.indirect_reservoirs_per_probe + 1);
	}

	out_specular[dispatch_thread_id] = float4(lighting, 1.0f);
}
