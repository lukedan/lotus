#include "math/sh.hlsli"

#include "shader_types.hlsli"
#include "probes.hlsli"

StructuredBuffer<indirect_lighting_reservoir> indirect_reservoirs : register(t0, space0);
RWTexture3D<float4>                           probe_sh0           : register(u1, space0);
RWTexture3D<float4>                           probe_sh1           : register(u2, space0);
RWTexture3D<float4>                           probe_sh2           : register(u3, space0);
RWTexture3D<float4>                           probe_sh3           : register(u4, space0);

ConstantBuffer<probe_constants>           probe_consts : register(b5, space0);
ConstantBuffer<summarize_probe_constants> constants    : register(b6, space0);

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

	float4 old_sh0 = probe_sh0[dispatch_thread_id];
	float4 old_sh1 = probe_sh1[dispatch_thread_id];
	float4 old_sh2 = probe_sh2[dispatch_thread_id];
	float4 old_sh3 = probe_sh3[dispatch_thread_id];

	float4 sh0 = lerp(old_sh0, float4(irr_r.x, irr_g.x, irr_b.x, 0.0f), constants.ra_alpha);
	float4 sh1 = lerp(old_sh1, float4(irr_r.y, irr_g.y, irr_b.y, 0.0f), constants.ra_alpha);
	float4 sh2 = lerp(old_sh2, float4(irr_r.z, irr_g.z, irr_b.z, 0.0f), constants.ra_alpha);
	float4 sh3 = lerp(old_sh3, float4(irr_r.w, irr_g.w, irr_b.w, 0.0f), constants.ra_alpha);
	
	probe_sh0[dispatch_thread_id] = sh0;
	probe_sh1[dispatch_thread_id] = sh1;
	probe_sh2[dispatch_thread_id] = sh2;
	probe_sh3[dispatch_thread_id] = sh3;
}
