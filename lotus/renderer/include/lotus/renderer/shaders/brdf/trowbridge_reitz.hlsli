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

	float g1(float n_dot_v, float alpha) {
		float sqr_alpha = squared(alpha);
		float sqr_n_v = squared(n_dot_v);
		return rcp(0.5f * (sqrt(1.0f - sqr_alpha + sqr_alpha / sqr_n_v) + 1.0f));
	}

	// "Sampling the GGX Distribution of Visible Normals" by Eric Heitz
	// v is view vector in tangent space, returns the half vector in the same tangent space
	float3 importance_sample_d_visible(float2 xi, float3 v, float alpha) {
		// transform into hemisphere configuration
		float3 vh = normalize(float3(alpha, alpha, 1.0f) * v);
		// orthonormal basis
		float lensq = dot(vh.xy, vh.xy);
		float3 t1 = lensq > 0.0f ? float3(-vh.y, vh.x, 0.0f) * rsqrt(lensq) : float3(1.0f, 0.0f, 0.0f);
		float3 t2 = cross(vh, t1);
		// parameterization of projected area
		float r = sqrt(xi.x);
		float phi = 2.0f * pi * xi.y;
		float t1l = r * cos(phi);
		float t2l = r * sin(phi);
		float s = 0.5f * (1.0f + vh.z);
		t2l = (1.0f - s) * sqrt(1.0f - squared(t1l)) + s * t2l;
		// reprojection onto the hemisphere
		float3 nh = t1l * t1 + t2l * t2 + sqrt(1.0f - t1l * t1l - t2l * t2l) * vh;
		// transform back to ellipsoid configuration
		float3 ne = normalize(float3(alpha * nh.x, alpha * nh.y, nh.z));
		return ne;
	}
	float importance_sample_d_visible_pdf(float n_dot_v, float n_dot_h, float v_dot_h, float alpha) {
		return g1(n_dot_v, alpha) * v_dot_h * d(n_dot_h, alpha) / n_dot_v;
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
