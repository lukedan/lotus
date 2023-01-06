#include "types.hlsli"
#include "common.hlsli"
#include "gbuffer_common.hlsli"
#include "lights.hlsli"
#include "utils/pcg32.hlsli"
#include "math/sh.hlsli"

#include "common_shaders/types.hlsli"
#include "shader_types.hlsli"
#include "probes.hlsli"
#include "shading.hlsli"

Texture2D<float4> gbuffer_albedo_glossiness : register(t0, space0);
Texture2D<float4> gbuffer_normal            : register(t1, space0);
Texture2D<float4> gbuffer_metalness         : register(t2, space0);
Texture2D<float>  gbuffer_depth             : register(t3, space0);

RWTexture2D<float4> out_diffuse  : register(u4, space0);
RWTexture2D<float4> out_specular : register(u5, space0);

StructuredBuffer<light>                     all_lights        : register(t6, space0);
StructuredBuffer<direct_lighting_reservoir> direct_reservoirs : register(t7, space0);
StructuredBuffer<probe_data>                indirect_probes   : register(t8, space0);

RaytracingAccelerationStructure rtas : register(t9, space0);

ConstantBuffer<lighting_constants> constants    : register(b10, space0);
ConstantBuffer<probe_constants>    probe_consts : register(b11, space0);

[numthreads(8, 8, 1)]
void main_cs(uint2 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= constants.screen_size)) {
		return;
	}

	float2 uv = (dispatch_thread_id + 0.5f) / (float2)constants.screen_size;
	g_buffer::contents gbuf = g_buffer::decode(
		gbuffer_albedo_glossiness[dispatch_thread_id],
		gbuffer_normal           [dispatch_thread_id],
		gbuffer_metalness        [dispatch_thread_id],
		gbuffer_depth            [dispatch_thread_id],
		uv, constants.inverse_jittered_projection_view, constants.depth_linearization_constants
	);
	float3 view_vec = normalize(constants.camera.xyz - gbuf.fragment.position_ws);

	pcg32::state rng = pcg32::seed(dispatch_thread_id.y * 50000 + dispatch_thread_id.x * 3, 0);

	float3 cell_pos = probes::coord_from_position(gbuf.fragment.position_ws, probe_consts);
	uint3 cell_coord = (uint3)clamp((int3)floor(cell_pos), 0, (int3)probe_consts.grid_size - 2);
	cell_coord = probes::get_random_coord(gbuf.fragment.position_ws, gbuf.fragment.normal_ws, cell_coord, probe_consts, pcg32::random_01(rng));

	uint use_probe_index = probes::coord_to_index(cell_coord, probe_consts);
	uint direct_reservoir_offset = use_probe_index * probe_consts.direct_reservoirs_per_probe;
	uint indirect_reservoir_offset = use_probe_index * probe_consts.indirect_reservoirs_per_probe;

	float3 diffuse_color = (float3)0;
	float3 specular_color = (float3)0;

	if (constants.lighting_mode == 1) {
		for (uint i = 0; i < probe_consts.direct_reservoirs_per_probe; ++i) {
			direct_lighting_reservoir cur_res = direct_reservoirs[direct_reservoir_offset + i];
			if (cur_res.light_index == 0) {
				continue;
			}

			light cur_li = all_lights[cur_res.light_index - 1];
			lights::derived_data light_data = lights::compute_derived_data(cur_li, gbuf.fragment.position_ws);

			if (constants.trace_shadow_rays_for_reservoir) { // trace shadow ray
				RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ray_query;
				RayDesc ray;
				ray.Origin    = gbuf.fragment.position_ws;
				ray.TMin      = 0.01f;
				ray.Direction = -light_data.direction;
				ray.TMax      = light_data.distance;
				ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
				ray_query.Proceed();
				if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
					continue;
				}
			}

			float3 d, s;
			shade_direct_light(gbuf.fragment, cur_li, light_data, view_vec, d, s);
			diffuse_color += cur_res.data.contribution_weight * d;
			specular_color += cur_res.data.contribution_weight * s;
		}
		diffuse_color /= probe_consts.direct_reservoirs_per_probe;
		specular_color /= probe_consts.direct_reservoirs_per_probe;
	} else if (constants.lighting_mode == 2) {
		for (uint i = 0; i < constants.num_lights; ++i) {
			light l = all_lights[i];
			lights::derived_data light_data = lights::compute_derived_data(l, gbuf.fragment.position_ws);

			if (constants.trace_shadow_rays_for_naive) { // trace shadow ray
				RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ray_query;
				RayDesc ray;
				ray.Origin    = gbuf.fragment.position_ws;
				ray.TMin      = 0.01f;
				ray.Direction = -light_data.direction;
				ray.TMax      = light_data.distance;
				ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
				ray_query.Proceed();
				if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
					continue;
				}
			}

			float3 d, s;
			shade_direct_light(gbuf.fragment, l, light_data, view_vec, d, s);
			diffuse_color += d;
			specular_color += s;
		}
	}

	diffuse_color *= constants.direct_diffuse_multiplier;
	specular_color *= constants.direct_specular_multiplier;

	if (constants.use_indirect) {
		probe_data probe_sh = indirect_probes[use_probe_index];
		sh::sh2 cosine_lobe = sh::clamped_cosine::eval_sh2(gbuf.fragment.normal_ws);
		float3 color = float3(
			sh::integrate((sh::sh2)probe_sh.irradiance_sh2_r, cosine_lobe),
			sh::integrate((sh::sh2)probe_sh.irradiance_sh2_g, cosine_lobe),
			sh::integrate((sh::sh2)probe_sh.irradiance_sh2_b, cosine_lobe)
		) / pi;
		diffuse_color += color * gbuf.fragment.albedo * (1.0f - gbuf.fragment.metalness);
	}

	out_diffuse [dispatch_thread_id] = float4(diffuse_color, 0.0f);
	out_specular[dispatch_thread_id] = float4(specular_color, 0.0f);
}
