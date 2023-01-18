#include "types.hlsli"

struct vs_input {
	float3 position : POSITION;
	float4 color    : COLOR0;
};

struct vs_output {
	float4 position : SV_Position;
	float4 color    : COLOR0;
};

struct ps_output {
	float4 color : SV_Target0;
};

ConstantBuffer<debug_draw_data> data : register(b0, space0);

vs_output main_vs(vs_input input) {
	vs_output result = (vs_output)0;
	result.position = mul(data.projection, float4(input.position, 1.0f));
	result.color    = input.color;
	return result;
}

ps_output main_ps(vs_output input) {
	ps_output result = (ps_output)0;
	result.color = input.color;
	return result;
}
