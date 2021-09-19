struct vs_input {
	float4 position : POSITION;
	float2 uv : TEXCOORD0;
};

struct ps_input {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

ps_input main_vs(vs_input input) {
	ps_input output = (ps_input)0;
	output.position = input.position;
	output.uv = input.uv;
	return output;
}

uniform Texture2D<float4> albedo : register(t0);
uniform SamplerState point_sampler : register(s1);

float4 main_ps(ps_input input) : SV_Target0 {
	return albedo.SampleLevel(point_sampler, input.uv, 0);
}
