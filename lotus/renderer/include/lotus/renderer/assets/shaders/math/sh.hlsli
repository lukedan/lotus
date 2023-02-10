#ifndef LOTUS_RENDERER_MATH_SH_HLSLI
#define LOTUS_RENDERER_MATH_SH_HLSLI

#include "common.hlsli"


namespace sh {
	namespace constants {
		static const float band0_factor = 0.5f / sqrt_pi;
		static const float band1_factor = 0.5f * sqrt_3 / sqrt_pi;

		static const float zh0_rotation_factor = 2.0f * sqrt_pi;
		static const float zh1_rotation_factor = 2.0f * sqrt_pi / sqrt_3;
	}

	struct band0 {
		float value;

		static band0 ctor(float v) {
			band0 result;
			result.value = v;
			return result;
		}
		static band0 coefficients() {
			return ctor(constants::band0_factor);
		}
		static band0 rotate_zh(float zh_v) {
			return coefficients() * constants::zh0_rotation_factor * zh_v;
		}

		band0 operator+(band0 rhs) {
			return ctor(value + rhs.value);
		}
		band0 operator-(band0 rhs) {
			return ctor(value - rhs.value);
		}
		band0 operator*(float rhs) {
			return ctor(value * rhs);
		}
	};
	struct band1 {
		float3 value;

		static band1 ctor(float3 v) {
			band1 result;
			result.value = v;
			return result;
		}
		static band1 ctor(float v0, float v1, float v2) {
			return ctor(float3(v0, v1, v2));
		}
		static band1 coefficients(float3 dir) {
			return ctor(-dir.y, dir.z, -dir.x) * constants::band1_factor;
		}
		static band1 rotate_zh(float zh_v, float3 z) {
			return coefficients(z) * constants::zh1_rotation_factor * zh_v;
		}

		band1 operator+(band1 rhs) {
			return ctor(value + rhs.value);
		}
		band1 operator-(band1 rhs) {
			return ctor(value - rhs.value);
		}
		band1 operator*(float rhs) {
			return ctor(value * rhs);
		}
	};

	float integrate(band0 lhs, band0 rhs) {
		return lhs.value * rhs.value;
	}
	float integrate(band1 lhs, band1 rhs) {
		return dot(lhs.value, rhs.value);
	}


	struct sh2 {
		band0 b0;
		band1 b1;

		static sh2 ctor(band0 b0v, band1 b1v) {
			sh2 result;
			result.b0 = b0v;
			result.b1 = b1v;
			return result;
		}
		static sh2 ctor(float4 v) {
			return ctor(band0::ctor(v.x), band1::ctor(v.yzw));
		}
		static sh2 ctor(float v0, float v10, float v11, float v12) {
			return ctor(float4(v0, v10, v11, v12));
		}

		sh2 operator+(sh2 rhs) {
			return ctor(b0 + rhs.b0, b1 + rhs.b1);
		}
		sh2 operator-(sh2 rhs) {
			return ctor(b0 - rhs.b0, b1 - rhs.b1);
		}
		sh2 operator*(float rhs) {
			return ctor(b0 * rhs, b1 * rhs);
		}

		static sh2 coefficients(float3 dir) {
			return ctor(band0::coefficients(), band1::coefficients(dir));
		}
	};

	float integrate(sh2 lhs, sh2 rhs) {
		return integrate(lhs.b0, rhs.b0) + integrate(lhs.b1, rhs.b1);
	}
}

namespace zh {
	struct zh2 {
		float2 value;

		static zh2 ctor(float2 v) {
			zh2 result;
			result.value = v;
			return result;
		}
		static zh2 ctor(float b0, float b1) {
			return ctor(float2(b0, b1));
		}

		sh::sh2 rotate(float3 z) {
			return sh::sh2::ctor(sh::band0::rotate_zh(value.x), sh::band1::rotate_zh(value.y, z));
		}
	};
	struct zh3 {
		float3 value;

		static zh3 ctor(float3 v) {
			zh3 result;
			result.value = v;
			return result;
		}
		static zh3 ctor(float b0, float b1, float b2) {
			return ctor(float3(b0, b1, b2));
		}
	};
}


namespace sh {
	namespace clamped_cosine {
		static const float band0_zh = 0.5f * sqrt_pi;
		static const float band1_zh = sqrt_pi / sqrt_3;

		band0 eval_band0() {
			return band0::rotate_zh(band0_zh);
		}
		band1 eval_band1(float3 v) {
			return band1::rotate_zh(band1_zh, v);
		}

		sh2 eval_sh2(float3 v) {
			return sh2::ctor(eval_band0(), eval_band1(v));
		}
	}

	namespace impulse {
		static const float band0_zh = 0.5f / sqrt_pi;
		static const float band1_zh = 0.5f * sqrt_3 / sqrt_pi;

		band0 eval_band0() {
			return band0::rotate_zh(band0_zh);
		}
		band1 eval_band1(float3 v) {
			return band1::rotate_zh(band1_zh, v);
		}

		sh2 eval_sh2(float3 v) {
			return sh2::ctor(eval_band0(), eval_band1(v));
		}
	}
}

#endif
