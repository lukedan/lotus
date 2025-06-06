#include "common.hlsli"
#include "types.hlsli"
#include "material_common.hlsli"
#include "utils/pcg32.hlsli"
#include "math/distribution.hlsli"
#include "math/sh.hlsli"
#include "lights.hlsli"

#include "common_shaders/types.hlsli"

#include "shader_types.hlsli"
#include "probes.hlsli"
#include "reservoir.hlsli"
#include "shading.hlsli"

ConstantBuffer<probe_constants>             probe_consts    : register(b0, space0);
ConstantBuffer<shade_point_debug_constants> constants       : register(b1, space0);
ConstantBuffer<lighting_constants>          lighting_consts : register(b2, space0);
StructuredBuffer<direct_lighting_reservoir> direct_probes   : register(t3, space0);
Texture3D<float4>                           indirect_sh0    : register(t4, space0);
Texture3D<float4>                           indirect_sh1    : register(t5, space0);
Texture3D<float4>                           indirect_sh2    : register(t6, space0);
Texture3D<float4>                           indirect_sh3    : register(t7, space0);
Texture2D<float2>                           envmap_lut      : register(t8, space0);
RWTexture2D<float4>                         out_irradiance  : register(u9, space0);
RWTexture2D<float3>                         out_accum       : register(u10, space0);
RaytracingAccelerationStructure             rtas            : register(t11, space0);

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

bool cast_ray(float3 pos, float3 dir, out material::shading_properties frag) {
	RayDesc ray;
	ray.Origin    = pos;
	ray.TMin      = 0.001f;
	ray.Direction = dir;
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

		frag = fetch_material_properties(
			hit_position,
			ray_query.CommittedPrimitiveIndex(),
			ray_query.CommittedTriangleBarycentrics(),
			ray_query.CommittedObjectToWorld3x4(),
			inst, geom, mat,
			textures, positions, normals, tangents, uvs, indices,
			linear_sampler
		);

		return true;
	}
	return false;
}

bool test_visibility(float3 pos, float3 dir, float len) {
	RayDesc ray;
	ray.Origin    = pos;
	ray.TMin      = 0.001f;
	ray.Direction = dir;
	ray.TMax      = len - 0.001f;

	RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
	ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
	ray_query.Proceed();

	return ray_query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
}

[numthreads(8, 8, 1)]
void main_cs(uint2 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= constants.window_size)) {
		return;
	}

	pcg32 rng = pcg32::seed(dispatch_thread_id.y * 3001 + dispatch_thread_id.x * 5, constants.num_frames);

	if (constants.mode == 4) {
		float3 total_light = (float3)0.0f;
		float3 attenuation = (float3)1.0f;
		float3 direction;

		bool terminated = false;
		material::shading_properties fragment;

		{ // primary ray
			direction = normalize((
				constants.top_left +
				(dispatch_thread_id.x + rng.next_01()) * constants.x +
				(dispatch_thread_id.y + rng.next_01()) * constants.y
			).xyz);

			if (!cast_ray(constants.camera.xyz, direction, fragment)) {
				terminated = true;
			}
		}

		for (uint i = 0; i < 20 && !terminated; ++i) {
			// random light sample
			uint light_index = rng.next_below_fast(constants.num_lights);
			light cur_li = all_lights[light_index];
			lights::derived_data li_data = lights::compute_derived_data(cur_li, fragment.position_ws);
			if (test_visibility(fragment.position_ws, -li_data.direction, li_data.distance)) {
				float3 ld, ls;
				shade_direct_light(fragment, cur_li, li_data, -direction, ld, ls);
				total_light += attenuation * constants.num_lights * (ld + ls);
			}

			// indirect
			float3 new_dir;
			float pdf;
			float alpha = squared(1.0f - fragment.glossiness);
			if (rng.next() & 1) {
				new_dir = normalize(fragment.normal_ws + distribution::unit_square_to_unit_sphere(float2(rng.next_01(), rng.next_01())));
			} else {
				tangent_frame ts = tangent_frame::any(fragment.normal_ws);
				float2 smp = trowbridge_reitz::importance_sample_d(rng.next_01(), alpha);
				float n_h = smp.x;
				float theta = 2.0f * pi * rng.next_01();
				float3 h = ts.normal * n_h + sqrt(1.0f - n_h * n_h) * (cos(theta) * ts.tangent + sin(theta) * ts.bitangent);
				new_dir = reflect(direction, h);
			}
			{
				float pdf1 = dot(new_dir, fragment.normal_ws) / pi;
				float3 h = normalize(new_dir - direction);
				float pdf2 = trowbridge_reitz::d(dot(fragment.normal_ws, h), alpha) / (4.0f * dot(new_dir, h));
				pdf = 0.5f * (pdf1 + pdf2);
			}

			float3 bd, bs;
			brdf(fragment, new_dir, -direction, bd, bs);
			attenuation *= (bd + bs) * dot(new_dir, fragment.normal_ws) / max(0.01f, pdf);
			if (!cast_ray(fragment.position_ws, new_dir, fragment)) {
				terminated = true;
			}
			direction = new_dir;
		}

		float3 accum;
		if (constants.num_frames > 1) {
			accum = (out_accum[dispatch_thread_id] += total_light);
		} else {
			accum = (out_accum[dispatch_thread_id] = total_light);
		}
		out_irradiance[dispatch_thread_id] = float4(accum / constants.num_frames, 1.0f);

		return;
	}

	float3 direction = normalize((constants.top_left + (dispatch_thread_id.x + 0.5f) * constants.x + (dispatch_thread_id.y + 0.5f) * constants.y).xyz);

	float3 irradiance = (float3)0.0f;
	material::shading_properties fragment = (material::shading_properties)0;

	if (cast_ray(constants.camera.xyz, direction, fragment)) {
		irradiance = shade_point(
			fragment, -direction, direct_probes, indirect_sh0, indirect_sh1, indirect_sh2, indirect_sh3, linear_clamp_sampler, all_lights,
			envmap_lut, lighting_consts.envmaplut_uvscale, lighting_consts.envmaplut_uvbias, probe_consts, rng
		);
	}

	float4 result = (float4)1.0f;
	if (constants.mode == 1) {
		result.rgb = irradiance;
	} else if (constants.mode == 2) {
		result.rgb = fragment.albedo;
	} else if (constants.mode == 3) {
		result.rgb = fragment.normal_ws * 0.5f + 0.5f;
	}
	out_irradiance[dispatch_thread_id] = result;
}
