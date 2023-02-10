#include "utils/pcg32.hlsli"

#include "shader_types.hlsli"
#include "probes.hlsli"
#include "reservoir.hlsli"

RaytracingAccelerationStructure                  rtas              : register(t0, space0);
StructuredBuffer<indirect_lighting_reservoir>    input_reservoirs  : register(t1, space0);
RWStructuredBuffer<indirect_lighting_reservoir>  output_reservoirs : register(u2, space0);
ConstantBuffer<probe_constants>                  probe_consts      : register(b3, space0);
ConstantBuffer<indirect_spatial_reuse_constants> constants         : register(b4, space0);

[numthreads(4, 4, 4)]
void main_cs(uint3 dispatch_thread_id : SV_DispatchThreadID) {
	float3 their_coord = dispatch_thread_id + uint3(constants.offset); // underflow is fine
	if (any(dispatch_thread_id >= probe_consts.grid_size)) {
		return;
	}

	uint my_index = probes::coord_to_index(dispatch_thread_id, probe_consts) * probe_consts.indirect_reservoirs_per_probe;
	if (any(their_coord >= probe_consts.grid_size)) {
		for (uint i = 0; i < probe_consts.indirect_reservoirs_per_probe; ++i) {
			output_reservoirs[my_index + i] = input_reservoirs[my_index + i];
		}
		return;
	}

	uint their_index = probes::coord_to_index(their_coord, probe_consts) * probe_consts.indirect_reservoirs_per_probe;

	float3 my_pos = probes::coord_to_position(dispatch_thread_id, probe_consts);
	float3 their_pos = probes::coord_to_position(their_coord, probe_consts);

	pcg32::state rng = pcg32::seed(dispatch_thread_id.x * 30001 + dispatch_thread_id.y * 503 + dispatch_thread_id.x * 7, constants.frame_index);

	bool visible = true;

	if (constants.visibility_test_mode == 1) {
		RayDesc ray;
		ray.Origin    = my_pos;
		ray.TMin      = 0.0f;
		ray.Direction = their_pos - my_pos;
		ray.TMax      = 1.0f;

		RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ray_query;
		ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
		ray_query.Proceed();

		if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
			visible = false;
		}
	}

	for (uint i = 0; i < probe_consts.indirect_reservoirs_per_probe; ++i) {
		indirect_lighting_reservoir my_res = input_reservoirs[my_index + i];
		indirect_lighting_reservoir their_res = input_reservoirs[their_index + i];

		bool cur_visible = visible;
		if (constants.visibility_test_mode == 2) {
			RayDesc ray;
			ray.Origin    = my_pos;
			ray.TMin      = 0.01f;
			if (their_res.distance >= max_float_v) {
				ray.Direction = octahedral_mapping::to_direction_unnormalized(their_res.direction_octahedral);
				ray.TMax      = max_float_v;
			} else {
				float3 offset = their_pos + octahedral_mapping::to_direction_normalized(their_res.direction_octahedral) * their_res.distance - my_pos;
				ray.Direction = offset;
				ray.TMax      = 0.99f;
			}

			RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ray_query;
			ray_query.TraceRayInline(rtas, 0, 0xFF, ray);
			ray_query.Proceed();

			if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
				cur_visible = false;
			}
		}

		if (cur_visible) {
			float their_pdf = max(max(their_res.irradiance.r, their_res.irradiance.g), their_res.irradiance.b);
			if (reservoirs::merge(my_res.data, their_res.data, their_pdf, pcg32::random_01(rng))) {
				my_res.irradiance = their_res.irradiance;
				if (their_res.distance >= max_float_v) {
					my_res.distance = their_res.distance;
					my_res.direction_octahedral = their_res.direction_octahedral;
				} else {
					float3 offset = their_pos + octahedral_mapping::to_direction_normalized(their_res.direction_octahedral) * their_res.distance - my_pos;
					my_res.distance = length(offset);
					my_res.direction_octahedral = octahedral_mapping::from_direction(offset / my_res.distance);
				}
			}
		}

		output_reservoirs[my_index + i] = my_res;
	}
}
