#include "utils/fullscreen_quad_vs.hlsl"

#include "shader_types.hlsli"

ConstantBuffer<lighting_blit_constants> constants : register(b0, space0);
Texture2D<float3> irradiance : register(t1, space0);

float4 main_ps(float4 pos : SV_Position) : SV_Target0 {
	uint2 coord = (uint2)pos.xy;
	float3 result = irradiance[coord];
	result *= constants.lighting_scale;
	if (any(isnan(result)) || any(isinf(result))) {
		result = float3(1.0f, 0.0f, 1.0f);
	}
	return float4(result, 1.0f);
}
