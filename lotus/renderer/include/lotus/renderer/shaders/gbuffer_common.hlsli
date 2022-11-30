#ifndef LOTUS_RENDERER_GBUFFER_COMMON_HLSLI
#define LOTUS_RENDERER_GBUFFER_COMMON_HLSLI

namespace g_buffer {
	struct contents {
		float2 uv;
		float3 albedo;
		float3 normal_ws;
		float  glossiness;
		float  metalness;
		float  raw_depth;
		float  linear_depth;
		float3 position_ws;
	};

	contents decode(
		float4 sample1, float4 sample2, float4 sample3, float4 sample4,
		float2 uv, float4x4 inverse_projection_view, float2 depth_linearize
	) {
		contents result = (contents)0;
		result.albedo     = sample1.rgb;
		result.glossiness = sample1.a;
		result.normal_ws  = sample2.xyz;
		result.metalness  = sample3.r;
		result.raw_depth  = sample4.r;

		result.uv           = uv;
		result.linear_depth = rcp(result.raw_depth * depth_linearize.x + depth_linearize.y);
		float4 position_cs  = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), result.raw_depth, 1.0f);
		result.position_ws  = mul(inverse_projection_view, position_cs).xyz * result.linear_depth;

		return result;
	}
}

#endif
