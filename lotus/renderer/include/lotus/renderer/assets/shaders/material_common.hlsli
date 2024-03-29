#ifndef LOTUS_RENDERER_MATERIAL_COMMON_HLSLI
#define LOTUS_RENDERER_MATERIAL_COMMON_HLSLI

#include "types.hlsli"

namespace material {
	struct basic_properties {
		float3 albedo;
		float  presence;
		float  glossiness;
		float  metalness;

		float3 normal_ts;
	};

	struct shading_properties {
		float3 albedo;
		float3 position_ws;
		float3 normal_ws;
		float  glossiness;
		float  metalness;
	};

	float3 evaluate_normal_mikkt(float3 normal_ts, float3 geom_normal_ws, float3 tangent_ws, float bitangent_sign) {
		float3 bitangent_ws = bitangent_sign * cross(geom_normal_ws, tangent_ws);
		return normalize(mul(transpose(float3x3(tangent_ws, bitangent_ws, geom_normal_ws)), normal_ts));
	}
}

#endif
