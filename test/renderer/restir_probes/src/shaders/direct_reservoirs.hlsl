#include "types.hlsli"
#include "pcg32.hlsli"
#include "lights.hlsli"

#include "probes.hlsli"
#include "reservoir.hlsli"

ConstantBuffer<probe_constants>                   probe_consts      : register(b0, space0);
ConstantBuffer<direct_reservoir_update_constants> constants         : register(b1, space0);
RWStructuredBuffer<direct_lighting_reservoir>     direct_reservoirs : register(u2, space0);
StructuredBuffer<light>                           all_lights        : register(t3, space0);
RaytracingAccelerationStructure                   rtas              : register(t4, space0);

[numthreads(4, 4, 4)]
void main_cs(uint3 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= probe_consts.grid_size)) {
		return;
	}

	uint first_reservoir = probes::coord_to_index(dispatch_thread_id, probe_consts) * probe_consts.direct_reservoirs_per_probe;
	float3 probe_position = probes::coord_to_position(dispatch_thread_id, probe_consts);

	pcg32::state rng = pcg32::seed(
		dispatch_thread_id.x * 7000000 + dispatch_thread_id.y * 5000 + dispatch_thread_id.z * 3,
		constants.frame_index
	);

	for (uint i = 0; i < probe_consts.direct_reservoirs_per_probe; ++i) {
		uint light_index = pcg32::random_below_fast(constants.num_lights, rng);
		light cur_light = all_lights[light_index];
		lights::derived_data light_data = lights::compute_derived_data(cur_light, probe_position);

		direct_lighting_reservoir cur_res = direct_reservoirs[first_reservoir + i];
		cur_res.data.num_samples = min(cur_res.data.num_samples, constants.sample_count_cap);
		reservoir_common new_res_data = cur_res.data;

		// test visibility
		RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ray_query;
		RayDesc ray;
		ray.Origin    = probe_position;
		ray.TMin      = 0.0f;
		ray.Direction = -light_data.direction;
		ray.TMax      = light_data.distance;
		ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
		ray_query.Proceed();
		if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
			light_data.attenuation = 0.0f;
		}

		float pdf =
			light_data.attenuation *
			max(cur_light.irradiance.r, max(cur_light.irradiance.g, cur_light.irradiance.b));
		if (reservoirs::add_sample(new_res_data, 1.0f / constants.num_lights, pdf, pcg32::random_01(rng))) {
			cur_res.light_index = light_index + 1;
		}
		cur_res.data = new_res_data;

		direct_reservoirs[first_reservoir + i] = cur_res;
	}
}
