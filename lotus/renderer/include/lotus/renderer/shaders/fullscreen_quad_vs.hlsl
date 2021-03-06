struct ps_input {
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

static const float2 uvs[3] = {
	float2(0.0f, 0.0f),
	float2(2.0f, 0.0f),
	float2(0.0f, 2.0f),
};

ps_input main_vs(uint vert_id : SV_VertexID) {
	float2 uv = uvs[vert_id];
	ps_input result;
	result.position = float4(uv * 2.0f - 1.0f, 0.5f, 1.0f);
	result.position.y = -result.position.y;
	result.uv = uv;
	return result;
}
