#ifndef LOTUS_RENDERER_COMMON_HLSLI
#define LOTUS_RENDERER_COMMON_HLSLI

static const float pi = 3.141592653589793;

static const int  max_int_v  =  0x7FFFFFFF;
static const int  min_int_v  = -0x80000000;
static const uint max_uint_v =  0xFFFFFFFFu;

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

#define LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(SPACE)    \
	SamplerState linear_sampler : register(s0, SPACE); \

#endif
