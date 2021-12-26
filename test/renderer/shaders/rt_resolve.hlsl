#include "brdf.hlsl"

struct global_data {
	uint frame_index;
};
Texture2D<float4> light    : register(t0);
SamplerState point_sampler : register(s1);
ConstantBuffer<global_data> globals : register(b2);

struct ps_input {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

ps_input main_vs(uint vert_id : SV_VertexID) {
	ps_input result;
	float2 pos = float2(vert_id == 1 ? 1.0f : 0.0f, vert_id == 2 ? 1.0f : 0.0f);
	result.position = float4(pos * 4.0f - 1.0f, 0.0f, 1.0f);
	result.uv = pos * 2.0f;
	result.uv.y = 1.0f - result.uv.y;
	return result;
}

float4 main_ps(ps_input input) : SV_Target0 {
	float3 color = light.SampleLevel(point_sampler, input.uv.xy, 0).rgb / (globals.frame_index + 1);
	if (any(isinf(color))) {
		color = float3(1.0f, 0.0f, 0.0f);
	}
	if (any(isnan(color))) {
		color = float3(1.0f, 0.0f, 1.0f);
	}
	return float4(color, 1.0f);
}
