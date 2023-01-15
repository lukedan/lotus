#ifndef LOTUS_RENDERER_BRDF_FRESNEL_HLSLI
#define LOTUS_RENDERER_BRDF_FRESNEL_HLSLI

#include "common.hlsli"

namespace fresnel {
	float dielectric_parallel(float eta_i, float eta_t, float cos_i, float cos_t) {
		return (eta_t * cos_i - eta_i * cos_t) / (eta_t * cos_i + eta_i * cos_t);
	}
	float dielectric_perpendicular(float eta_i, float eta_t, float cos_i, float cos_t) {
		return (eta_i * cos_i - eta_t * cos_t) / (eta_i * cos_i + eta_t * cos_t);
	}
	float dielectric(float eta_i, float eta_t, float cos_i, float cos_t) {
		return 0.5f * (
			squared(dielectric_parallel     (eta_i, eta_t, cos_i, cos_t)) +
			squared(dielectric_perpendicular(eta_i, eta_t, cos_i, cos_t))
		);
	}

	float3 schlick(float3 r0, float cos_i) {
		float t = 1.0f - cos_i;
		return r0 + (1.0f - r0) * squared(squared(t)) * t;
	}
}

#endif
