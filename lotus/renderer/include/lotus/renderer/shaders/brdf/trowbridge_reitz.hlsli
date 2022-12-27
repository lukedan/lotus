#ifndef LOTUS_RENDERER_BRDF_TROWBRIDGE_REITZ_HLSLI
#define LOTUS_RENDERER_BRDF_TROWBRIDGE_REITZ_HLSLI

#include "common.hlsli"

namespace trowbridge_reitz {
	float d(float n_dot_h, float alpha) {
		float a2 = squared(alpha);
		float t = squared(n_dot_h) * (a2 - 1.0f) + 1.0f;
		return a2 * rcp(pi * squared(t));
	}

	// importance sample D times N dot H, returns the sampled N dot H value and pdf
	// xi == 0 yields H = N
	float2 importance_sample_d(float xi, float alpha) {
		float denom = xi * (squared(alpha) - 1.0f) + 1.0f;
		return float2(
			sqrt((1.0f - xi) * rcp(denom)),
			squared(denom) / (pi * squared(alpha))
		);
	}

	/*float d_anisotropic(float n_dot_h, float n_dot_t, float n_dot_b, float alpha_x, float alpha_y) {
		// TODO
	}*/

	float g2_smith(float n_dot_l, float n_dot_v, float alpha) {
		float a2 = squared(alpha);
		return
			2.0f * n_dot_l * n_dot_v / (
				n_dot_v * sqrt(a2 + (1.0f - a2) * squared(n_dot_l)) +
				n_dot_l * sqrt(a2 + (1.0f - a2) * squared(n_dot_v))
			);
	}

	float g2_smith_approximated(float n_dot_l, float n_dot_v, float alpha) {
		return
			2.0f * n_dot_l * n_dot_v /
			lerp(2.0f * n_dot_l * n_dot_v, n_dot_l + n_dot_v, alpha);
	}
}

#endif
