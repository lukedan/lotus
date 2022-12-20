#include "math/sh.hlsli"

#include "shader_types.hlsli"
#include "probes.hlsli"

StructuredBuffer<indirect_lighting_reservoir> indirect_reservoirs : register(t0, space0);
RWStructuredBuffer<probe_data>                probe_sh            : register(u1, space0);

ConstantBuffer<probe_constants> probe_consts : register(b2, space0);

[numthreads(4, 4, 4)]
void main_cs(uint3 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id >= probe_consts.grid_size)) {
		return;
	}

	uint probe_index = probes::coord_to_index(dispatch_thread_id, probe_consts);
	uint reservoir_offset = probe_index * probe_consts.indirect_reservoirs_per_probe;
	float4 irr_r = (float4)0.0f;
	float4 irr_g = (float4)0.0f;
	float4 irr_b = (float4)0.0f;
	for (uint i = 0; i < probe_consts.indirect_reservoirs_per_probe; ++i) {
		indirect_lighting_reservoir cur_res = indirect_reservoirs[reservoir_offset + i];
		float3 scale = cur_res.irradiance * (cur_res.data.contribution_weight / probe_consts.indirect_reservoirs_per_probe);
		sh::sh2 direction = sh::impulse::eval_sh2(normalize(
			octahedral_mapping::to_direction_unnormalized(cur_res.direction_octahedral)
		));
		irr_r += scale.r * (float4)direction;
		irr_g += scale.g * (float4)direction;
		irr_b += scale.b * (float4)direction;
	}

	probe_data result;
	result.irradiance_sh2_r = irr_r;
	result.irradiance_sh2_g = irr_g;
	result.irradiance_sh2_b = irr_b;
	probe_sh[probe_index] = result;
}
