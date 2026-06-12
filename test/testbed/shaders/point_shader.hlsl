#include "shader_types.hlsli"

struct vs_input {
	float3 position : POSITION;
	float4 color    : COLOR;
	float  size     : SIZE;
};

struct ps_input {
	float4 position : SV_Position;
	float4 color    : COLOR;
	float2 uv       : UV;
};

ConstantBuffer<point_shader_constants> constants;

ps_input main_vs(vs_input input, uint vertex_id : SV_VertexID) {
	const float2 quad_vertices[] = {
		float2(-1.0f, -1.0f),
		float2(-1.0f,  1.0f),
		float2( 1.0f, -1.0f),
		float2( 1.0f,  1.0f)
	};

	const float depth = mul(constants.view, float4(input.position, 1.0f)).z;
	const float2 quad_vert = quad_vertices[vertex_id];
	const float3 offset_ws = quad_vert.x * constants.right + quad_vert.y * constants.down;
	const float3 pos_ws = input.position + input.size * depth * offset_ws;

	ps_input result;
	result.position = mul(constants.projection_view, float4(pos_ws, 1.0f));
	result.color    = input.color * float4(1.0f, 1.0f, 1.0f, constants.opacity_multiplier);
	result.uv       = quad_vert;
	return result;
}

float4 main_ps(ps_input input) : SV_Target0 {
	if (dot(input.uv, input.uv) >= 1.0f) {
		discard;
	}
	return input.color;
}
