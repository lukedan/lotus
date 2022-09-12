#ifndef LOTUS_RENDERER_MATERIAL_COMMON_HLSLI
#define LOTUS_RENDERER_MATERIAL_COMMON_HLSLI

#include "types.hlsli"

namespace material {
	struct basic_properties {
		float3 albedo;
		float  glossiness;
		float  metalness;

		float3 normal_ts;
	};

	float3 evaluate_normal_mikkt(float3 normal_ts, float3 normal_ws, float3 tangent_ws, float bitangent_sign) {
		float3 bitangent_ws = bitangent_sign * cross(normal_ws, tangent_ws);
		return normalize(mul(transpose(float3x3(tangent_ws, bitangent_ws, normal_ws)), normal_ts));
	}
}

#endif
