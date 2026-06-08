#include "common.hlsli"
#include "shader_types.hlsli"

struct vs_input {
	float3 position : POSITION;
	float4 color    : COLOR;
	float3 normal   : NORMAL;
};

struct ps_input {
	float4 position    : SV_Position;
	float4 color       : COLOR;
	float3 normal      : NORMAL;
	float3 position_ws : POSITION_WS;
};

ConstantBuffer<default_shader_constants> constants : register(b0, space0);

ps_input main_vs(vs_input input) {
	ps_input result;
	result.position    = mul(constants.projection_view, float4(input.position, 1.0f));
	result.color       = input.color;
	result.normal      = input.normal;
	result.position_ws = input.position;
	return result;
}

float3 main_ps(ps_input input, bool front_facing : SV_IsFrontFace) : SV_Target0 {
	if (!front_facing) {
		input.normal *= -1.0f;
	}
	const float n_dot_l = dot(normalize(input.normal), constants.light_direction);
	const float lighting = n_dot_l * 0.5f + 0.5f;
	return lerp(1.0f, lighting, input.color.a) * input.color.rgb;
}
