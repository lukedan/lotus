#include "utils/fullscreen_quad_vs.hlsl"

#include "shader_types.hlsli"

Texture2D<float4> gbuffer_albedo_glossiness : register(t0, space0);
Texture2D<float4> gbuffer_normal            : register(t1, space0);
Texture2D<float4> gbuffer_metalness         : register(t2, space0);
Texture2D<float>  gbuffer_depth             : register(t3, space0);

ConstantBuffer<gbuffer_visualization_constants> constants : register(b4, space0);

float4 main_ps(float4 pos : SV_Position) : SV_Target0 {
	if (constants.exclude_sky) {
		float depth = gbuffer_depth[pos.xy];
		if (depth == 0.0f) {
			return float4(0.0f, 0.0f, 1.0f, 1.0f);
		}
	}
	if (constants.mode == 1) {
		return float4(gbuffer_albedo_glossiness[pos.xy].rgb, 1.0f);
	} else if (constants.mode == 2) {
		return float4(gbuffer_albedo_glossiness[pos.xy].aaa, 1.0f);
	} else if (constants.mode == 3) {
		return float4(gbuffer_normal[pos.xy].rgb * 0.5f + 0.5f, 1.0f);
	} else if (constants.mode == 4) {
		return float4(gbuffer_metalness[pos.xy].rrr, 1.0f);
	} else {
		return float4(1.0f, 0.0f, 1.0f, 1.0f);
	}
}
