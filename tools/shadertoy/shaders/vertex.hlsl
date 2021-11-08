struct ps_input {
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

ps_input main_vs(uint vert_id : SV_VertexID) {
	float2 uv = float2(vert_id & 1, (vert_id & 2) >> 1);
	ps_input result;
	result.position = float4(uv * 2.0f - 1.0f, 0.5f, 1.0f);
	result.position.y = -result.position.y;
	result.uv = uv;
	return result;
}
