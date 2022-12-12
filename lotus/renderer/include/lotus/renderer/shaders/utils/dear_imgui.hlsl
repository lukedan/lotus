#include "common.hlsli"
#include "types.hlsli"

struct vs_input {
	float2 position : POSITION;
	float4 color    : COLOR0;
	float2 uv       : TEXCOORD0;
};

struct vs_output {
	float4 position : SV_Position;
	float4 color    : COLOR0;
	float2 uv       : TEXCOORD0;
};

struct ps_output {
	float4 color : SV_Target0;
};

ConstantBuffer<dear_imgui_draw_data> data : register(b0, space0);
Texture2D<float4>                    tex  : register(t1, space0);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space1);

vs_output main_vs(vs_input input) {
	vs_output result = (vs_output)0;
	result.position = mul(data.projection, float4(input.position, 0.0f, 1.0f));
	result.color    = input.color;
	result.uv       = input.uv;
	return result;
}

ps_output main_ps(vs_output input) {
	if (any(input.position.xy < data.scissor_min) || any(input.position.xy > data.scissor_max)) {
		discard;
	}

	ps_output result = (ps_output)0;
	result.color = input.color;
	if (data.uses_texture) {
		result.color *= tex.Sample(linear_sampler, input.uv);
	}
	return result;
}
