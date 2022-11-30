#ifndef LOTUS_RENDERER_BRDF_TROWBRIDGE_REITZ_HLSLI
#define LOTUS_RENDERER_BRDF_TROWBRIDGE_REITZ_HLSLI

#include "common.hlsli"

namespace trowbridge_reitz {
	float d(float n_dot_h, float alpha) {
		float a2 = squared(alpha);
		float t = squared(n_dot_h) * (a2 - 1.0f) + 1.0f;
		return a2 * rcp(pi * squared(t));
	}

	// importance sample D times N dot H, returns the sampled N dot H value
	// xi == 0 yields H = N
	float importance_sample_d(float xi, float alpha) {
		return sqrt((1.0f - xi) * rcp(xi * (squared(alpha) - 1.0f) + 1.0f));
	}

	float d_anisotropic(float n_dot_h, float n_dot_t, float n_dot_b, float alpha_x, float alpha_y) {
		// TODO
	}
}

#endif
