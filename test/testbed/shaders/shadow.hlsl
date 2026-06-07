#include "shader_types.hlsli"

ConstantBuffer<shadow_constants> constants;

struct vs_input {
	float3 position : POSITION;
};

float4 main_vs(vs_input input) : SV_Position {
	return mul(constants.projection_view, float4(input.position, 1.0f));
}
