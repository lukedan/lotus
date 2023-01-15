#ifndef LOTUS_RENDERER_MATH_DISTRIBUTION_HLSLI
#define LOTUS_RENDERER_MATH_DISTRIBUTION_HLSLI

#include "common.hlsli"

namespace distribution {
	float3 unit_square_to_unit_sphere(float2 xi) {
		float theta = xi.x * 2.0f * pi;
		float z = 1.0f - 2.0f * xi.y;
		float xy_scale = sqrt(1.0f - squared(z));
		return float3(float2(sin(theta), cos(theta)) * xy_scale, z);
	}
}

#endif
