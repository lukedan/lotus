#ifndef LOTUS_RENDERER_COMMON_HLSLI
#define LOTUS_RENDERER_COMMON_HLSLI

static const float pi      = 3.141592653589793;
static const float sqrt_pi = 1.772453850905516;
static const float sqrt_2  = 1.414213562373095;
static const float sqrt_3  = 1.732050807568877;

static const int   max_int_v   =  0x7FFFFFFF;
static const int   min_int_v   = -0x80000000;
static const uint  max_uint_v  =  0xFFFFFFFFu;
static const float max_float_v = 3.4028234664e+38f;

float squared(float x) {
	return x * x;
}

namespace tangent_frame {
	struct tbn {
		float3 tangent;
		float3 bitangent;
		float3 normal;
	};

	// https://graphics.pixar.com/library/OrthonormalB/paper.pdf
	tbn any(float3 n) {
		float sign = n.z > 0.0f ? 1.0f : -1.0f;
		float a = -rcp(sign + n.z);
		float b = n.x * n.y * a;
		tbn result;
		result.tangent   = float3(1.0f + sign * squared(n.x) * a, sign * b, -sign * n.x);
		result.bitangent = float3(b, sign + squared(n.y) * a, -n.y);
		result.normal    = n;
		return result;
	}
}

// values are in [-1, 1]
namespace octahedral_mapping {
	float2 from_direction(float3 dir_normalized) {
		float3 sgn = sign(dir_normalized);
		float3 octa = dir_normalized / dot(abs(dir_normalized), 1.0f);
		if (octa.z < 0.0f) {
			octa.xy = sgn.xy * (1.0f - abs(octa.yx));
		}
		return octa.xy;
	}

	float3 to_direction_unnormalized(float2 octa) {
		float3 pos = float3(octa, 1.0f - dot(abs(octa.xy), 1.0f));
		if (pos.z < 0.0f) {
			pos.xy = sign(pos.xy) * (1.0f - abs(pos.xy));
		}
		return pos;
	}
}

#define LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(SPACE)    \
	SamplerState linear_sampler : register(s0, SPACE); \

#endif
