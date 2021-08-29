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

float4 main_ps(ps_input input) : SV_Target0 {
	return float4(input.uv, 1.0f - input.uv.x - input.uv.y, 1.0f);
}
