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

ConstantBuffer<default_shader_constants> constants  : register(b0, space0);
Texture2D<float>                         shadow_map : register(t1, space0);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space1);

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

	// sample shadow map
	float4 shadow_pos = mul(constants.shadow_projection_view, float4(input.position_ws, 1.0f));
	shadow_pos.xyz /= shadow_pos.w;
	const float2 shadow_uv = shadow_pos.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
	const bool shadow_valid = all(saturate(shadow_uv) == shadow_uv) && shadow_pos.z >= 0.0f;
	const float shadow =
		shadow_valid ?
		shadow_map.SampleCmpLevelZero(linear_clamp_greater_sampler, shadow_uv, shadow_pos.z) :
		1.0f;

	float lighting = 0.5f - 0.5f * dot(normalize(input.normal), constants.light_direction);
	lighting *= lerp(0.5f, 1.0f, shadow);
	return lerp(1.0f, lighting, input.color.a) * input.color.rgb;
}
