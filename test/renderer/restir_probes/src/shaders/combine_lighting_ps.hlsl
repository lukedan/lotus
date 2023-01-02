#include "common.hlsli"
#include "utils/fullscreen_quad_vs.hlsl"

#include "shader_types.hlsli"

Texture2D<float3> diffuse_lighting  : register(t0, space0);
Texture2D<float3> specular_lighting : register(t1, space0);
Texture2D<float3> indirect_specular : register(t2, space0);
ConstantBuffer<lighting_combine_constants> constants : register(b3, space0);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space1);

float3 main_ps(float4 pos : SV_Position) : SV_Target0 {
	float3 diffuse = diffuse_lighting[pos.xy];
	float3 specular = specular_lighting[pos.xy];
	float3 indirect_spec = indirect_specular[pos.xy];

	float3 result = diffuse + specular;
	if (constants.use_indirect_specular) {
		result += indirect_spec;
	}
	result *= constants.lighting_scale;

	if (any(isnan(result)) || any(isinf(result))) {
		result = float3(1.0f, 0.0f, 1.0f);
	}
	return result;
}
