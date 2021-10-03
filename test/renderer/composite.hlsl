struct ps_input {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

Texture2D<float4> base_color_metalness : register(t0);
Texture2D<float4> normal : register(t1);
Texture2D<float> gloss : register(t2);
Texture2D<float> depth : register(t3);
SamplerState point_sampler : register(s4);

ps_input main_vs(uint vert_id : SV_VertexID) {
	ps_input result;
	float2 pos = float2(vert_id == 1 ? 1.0f : 0.0f, vert_id == 2 ? 1.0f : 0.0f);
	result.position = float4(pos * 4.0f - 1.0f, 0.0f, 1.0f);
	result.uv = pos * 2.0f;
	result.uv.y = 1.0f - result.uv.y;
	return result;
}

float4 main_ps(ps_input input) : SV_Target0 {
	/*return float4(normal.SampleLevel(point_sampler, input.uv.xy, 0).xyz * 0.5f + 0.5f, 1.0f);*/
	return float4(base_color_metalness.SampleLevel(point_sampler, input.uv.xy, 0).rgb, 1.0f);
}
