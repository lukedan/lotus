#ifndef LOTUS_RENDERER_LIGHTS_HLSLI
#define LOTUS_RENDERER_LIGHTS_HLSLI

#include "common.hlsli"
#include "types.hlsli"

namespace lights {
	struct derived_data {
		float3 direction; // normalized
		float distance;
		float attenuation;
	};

	derived_data compute_derived_data(light li, float3 position) {
		derived_data result = (derived_data)0.0f;
		result.attenuation = 1.0f;
		if (li.type == light_type::directional_light) {
			result.direction = li.direction; // assumes normalized
			result.distance  = max_float_v;
		} else {
			float3 off = position - li.position;
			float sqr_dist = dot(off, off);
			float rcp_sqr_dist = rcp(sqr_dist);
			result.direction   = off * sqrt(rcp_sqr_dist);
			result.attenuation = rcp_sqr_dist;
			result.distance    = sqrt(sqr_dist);
		}
		return result;
	}
}

#endif
