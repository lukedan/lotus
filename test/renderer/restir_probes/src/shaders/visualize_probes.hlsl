#include "math/sh.hlsli"

#include "shader_types.hlsli"
#include "probes.hlsli"

static const float2 vertex_uv[6] = {
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),

	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
};

ConstantBuffer<probe_constants>            probe_consts : register(b0, space0);
ConstantBuffer<visualize_probes_constants> constants    : register(b1, space0);
Texture3D<float4>                          probe_sh0    : register(t2, space0);
Texture3D<float4>                          probe_sh1    : register(t3, space0);
Texture3D<float4>                          probe_sh2    : register(t4, space0);
Texture3D<float4>                          probe_sh3    : register(t5, space0);

struct ps_input {
	float4 pos : SV_Position;
	float2 uv  : TEXCOORD;
	uint instance_id : SV_InstanceID;
};

uint3 get_probe_coord_from_flat_id(uint id, uint3 grid_size) {
	return uint3(
		id % grid_size.x,
		(id / grid_size.x) % grid_size.y,
		id / (grid_size.x * grid_size.y)
	);
}

ps_input main_vs(uint instance_id : SV_InstanceID, uint vertex_id : SV_VertexID) {
	uint3 probe_coord = get_probe_coord_from_flat_id(instance_id, probe_consts.grid_size);
	float2 uv = vertex_uv[vertex_id];
	float3 position = probes::coord_to_position(probe_coord, probe_consts);
	position += constants.size * ((uv.x * 2.0f - 1.0f) * constants.unit_right + (uv.y * 2.0f - 1.0f) * constants.unit_down);

	ps_input result = (ps_input)0;
	result.uv  = uv;
	result.pos = mul(constants.projection_view, float4(position, 1.0f));
	result.instance_id = instance_id;
	return result;
}

float4 main_ps(ps_input input) : SV_Target0 {
	float2 normal_xy = input.uv * 2.0f - 1.0f;
	float normal_xy_len = length(normal_xy);
	if (normal_xy_len > 1.0f) {
		discard;
	}
	float3 normal = float3(normal_xy, 1.0f - normal_xy_len);
	normal = normal.x * constants.unit_right + normal.y * constants.unit_down - normal.z * constants.unit_forward;

	uint3 probe_coord = get_probe_coord_from_flat_id(input.instance_id, probe_consts.grid_size);
	float4 sh0 = probe_sh0[probe_coord];
	float4 sh1 = probe_sh1[probe_coord];
	float4 sh2 = probe_sh2[probe_coord];
	float4 sh3 = probe_sh3[probe_coord];
	sh::sh2 sh_r = sh::sh2::ctor(sh0.r, sh1.r, sh2.r, sh3.r);
	sh::sh2 sh_g = sh::sh2::ctor(sh0.g, sh1.g, sh2.g, sh3.g);
	sh::sh2 sh_b = sh::sh2::ctor(sh0.b, sh1.b, sh2.b, sh3.b);

	sh::sh2 conv;
	if (constants.mode == 1) {
		normal = reflect(constants.unit_forward, normal);
		conv = sh::sh2::coefficients(normal);
	} else if (constants.mode == 2) {
		conv = sh::clamped_cosine::eval_sh2(normal);
	} else if (constants.mode == 3) {
		return float4(normal * 0.5f + 0.5f, 1.0f);
	}
	float3 lighting = float3(sh::integrate(sh_r, conv), sh::integrate(sh_g, conv), sh::integrate(sh_b, conv));

	return float4(lighting * constants.lighting_scale, 1.0f);
}
