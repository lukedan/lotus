#include "common.hlsli"
#include "utils/fullscreen_quad_vs.hlsl"

#include "shader_types.hlsli"

Texture2D<float3> diffuse_lighting  : register(t0, space0);
Texture2D<float3> specular_lighting : register(t1, space0);
ConstantBuffer<lighting_combine_constants> constants : register(b2, space0);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space1);

float3 main_ps(float4 pos : SV_Position) : SV_Target0 {
	float3 diffuse = diffuse_lighting[pos.xy];
	float3 specular = specular_lighting[pos.xy];
	return (diffuse + specular) * constants.lighting_scale;
}
