#include "shader_types.hlsli"

struct vs_input {
	float3 position : position;
	float4 color    : COLOR;
};

struct ps_input {
	float4 position : SV_Position;
	float4 color    : COLOR;
};

ConstantBuffer<line_shader_constants> constants;

ps_input main_vs(vs_input input) {
	ps_input result;
	result.position = mul(constants.projection_view, float4(input.position, 1.0f));
	result.color    = input.color * float4(1.0f, 1.0f, 1.0f, constants.opacity_multiplier);
	return result;
}

float4 main_ps(ps_input input) : SV_Target0 {
	return input.color;
}
