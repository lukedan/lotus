#include "shader_types.hlsli"

struct vs_input {
	float3 position : POSITION;
	float4 color    : COLOR;
	float3 normal   : NORMAL;
};

struct ps_input {
	float4 position : SV_Position;
	float4 color    : COLOR;
	float3 normal   : NORMAL;
};

ConstantBuffer<default_shader_constants> constants;

ps_input main_vs(vs_input input) {
	ps_input result;
	result.position = mul(constants.projection_view, float4(input.position, 1.0f));
	result.color    = input.color;
	result.normal   = input.normal;
	return result;
}

float3 main_ps(ps_input input, bool front_facing : SV_IsFrontFace) : SV_Target0 {
	float lighting = 0.5f - 0.5f * dot(normalize(input.normal), constants.light_direction);
	if (!front_facing) {
		lighting = 1.0f - lighting;
	}
	return lerp(1.0f, lighting, input.color.a) * input.color.rgb;
}
