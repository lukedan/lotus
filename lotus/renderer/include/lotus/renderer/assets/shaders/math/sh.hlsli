#ifndef LOTUS_RENDERER_MATH_SH_HLSLI
#define LOTUS_RENDERER_MATH_SH_HLSLI

#include "common.hlsli"

namespace zh {
	typedef float2 zh2;
	typedef float3 zh3;
}

namespace sh {
	namespace constants {
		static const float band0_factor = 0.5f / sqrt_pi;
		static const float band1_factor = 0.5f * sqrt_3 / sqrt_pi;

		static const float zh0_rotation_factor = 2.0f * sqrt_pi;
		static const float zh1_rotation_factor = 2.0f * sqrt_pi / sqrt_3;
	}

	typedef float band0;
	typedef float3 band1;

	band0 coefficients_band0(float3 dir) {
		return constants::band0_factor;
	}
	band1 coefficients_band1(float3 dir) {
		return constants::band1_factor * float3(-dir.y, dir.z, -dir.x);
	}

	float integrate(band0 lhs, band0 rhs) {
		return lhs * rhs;
	}
	float integrate(band1 lhs, band1 rhs) {
		return dot(lhs, rhs);
	}

	band0 rotate_zh_band0(float zh_v, float3 z) {
		return constants::zh0_rotation_factor * zh_v * coefficients_band0(z);
	}
	band1 rotate_zh_band1(float zh_v, float3 z) {
		return constants::zh1_rotation_factor * zh_v * coefficients_band1(z);
	}


	struct sh2 {
		band0 b0;
		band1 b1;

		sh2 operator+(sh2 rhs) {
			sh2 result;
			result.b0 = b0 + rhs.b0;
			result.b1 = b1 + rhs.b1;
			return result;
		}
		sh2 operator-(sh2 rhs) {
			sh2 result;
			result.b0 = b0 - rhs.b0;
			result.b1 = b1 - rhs.b1;
			return result;
		}
		sh2 operator*(float rhs) {
			sh2 result;
			result.b0 = b0 * rhs;
			result.b1 = b1 * rhs;
			return result;
		}
	};

	sh2 coefficients_sh2(float3 dir) {
		return (sh2)float4(coefficients_band0(dir), coefficients_band1(dir));
	}

	sh2 rotate_zh(zh::zh2 v, float3 z) {
		return (sh2)float4(rotate_zh_band0(v.x, z), rotate_zh_band1(v.y, z));
	}

	float integrate(sh2 lhs, sh2 rhs) {
		return integrate(lhs.b0, rhs.b0) + integrate(lhs.b1, rhs.b1);
	}


	namespace clamped_cosine {
		static const float band0_zh = 0.5f * sqrt_pi;
		static const float band1_zh = sqrt_pi / sqrt_3;

		band0 eval_band0(float3 v) {
			return rotate_zh_band0(band0_zh, v);
		}
		band1 eval_band1(float3 v) {
			return rotate_zh_band1(band1_zh, v);
		}

		sh2 eval_sh2(float3 v) {
			return (sh2)float4(eval_band0(v), eval_band1(v));
		}
	}

	namespace impulse {
		static const float band0_zh = 0.5f / sqrt_pi;
		static const float band1_zh = 0.5f * sqrt_3 / sqrt_pi;

		band0 eval_band0(float3 v) {
			return rotate_zh_band0(band0_zh, v);
		}
		band1 eval_band1(float3 v) {
			return rotate_zh_band1(band1_zh, v);
		}

		sh2 eval_sh2(float3 v) {
			return (sh2)float4(eval_band0(v), eval_band1(v));
		}
	}
}

#endif
