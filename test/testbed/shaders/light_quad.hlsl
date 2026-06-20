#include "shader_types.hlsli"

ConstantBuffer<light_constants> constants  : register(b0, space0);
Texture2D<float3>               color_tex  : register(t1, space0);
Texture2D<float3>               normal_tex : register(t2, space0);
Texture2D<float>                ssao       : register(t3, space0);

float4 main_ps(float4 position : SV_Position) : SV_Target0 {
	const uint2 coord = (uint2)position;
	const float3 color = color_tex[coord];
	const float3 normal = normal_tex[coord];

	const float lighting =
		all(constants.light_direction == 0.0f) ?
		ssao[coord] : // ambient light
		saturate(dot(normal, constants.light_direction)); // directional light
	return float4(color * constants.light_color * lighting, 1.0f);
}
