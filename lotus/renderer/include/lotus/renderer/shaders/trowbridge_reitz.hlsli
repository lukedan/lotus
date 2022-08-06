#ifndef LOTUS_RENDERER_TROWBRIDGE_REITZ_HLSLI
#define LOTUS_RENDERER_TROWBRIDGE_REITZ_HLSLI

float trowbridge_reitz_d(float n_dot_h, float alpha) {
	float a2 = squared(alpha);
	float t = squared(n_dot_h) * (a2 - 1.0f) + 1.0f;
	return a2 * rcp(pi * squared(t));
}

float trowbridge_reitz_d_anisotropic(float n_dot_h, float n_dot_t, float n_dot_b, float alpha_x, float alpha_y) {
	// TODO
}

#endif
