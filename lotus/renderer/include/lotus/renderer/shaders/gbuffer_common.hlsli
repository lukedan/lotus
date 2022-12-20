#ifndef LOTUS_RENDERER_GBUFFER_COMMON_HLSLI
#define LOTUS_RENDERER_GBUFFER_COMMON_HLSLI

#include "material_common.hlsli"

namespace g_buffer {
	struct contents {
		material::shading_properties fragment;

		float2 uv;
		float  raw_depth;
		float  linear_depth;
	};

	contents decode(
		float4 sample1, float4 sample2, float4 sample3, float4 sample4,
		float2 uv, float4x4 inverse_projection_view, float2 depth_linearize
	) {
		contents result = (contents)0;
		result.fragment.albedo     = sample1.rgb;
		result.fragment.glossiness = sample1.a;
		result.fragment.normal_ws  = sample2.xyz;
		result.fragment.metalness  = sample3.r;
		result.raw_depth = sample4.r;

		result.uv           = uv;
		result.linear_depth = rcp(result.raw_depth * depth_linearize.x + depth_linearize.y);

		float4 position_cs = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), result.raw_depth, 1.0f);
		result.fragment.position_ws  = mul(inverse_projection_view, position_cs).xyz * result.linear_depth;

		return result;
	}
}

#endif
