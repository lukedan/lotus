#ifndef __BRDF_HLSL__
#define __BRDF_HLSL__

#include "misc.hlsl"

float schlick_fresnel(float u) {
	float m = saturate(1.0f - u);
	return sqr(sqr(m)) * m; // m^5
}
float gtr1(float n_h, float a) {
	if (a >= 1.0f) {
		return 1.0f / pi;
	}
	float a2 = sqr(a);
	float t = 1.0f + (a2 - 1.0f) * sqr(n_h);
	return (a2 - 1.0f) / (pi * log(a2) * t);
}
float gtr2(float n_h, float a) {
	float a2 = sqr(a);
	float t = 1.0f + (a2 - 1.0f) * sqr(n_h);
	return a2 / (pi * sqr(t));
}
float smith_g_ggx(float n_v, float alpha) {
	float a = sqr(alpha);
	float b = sqr(n_v);
	return rcp(n_v + sqrt(a + b - a * b));
}

float3 disney_brdf(float3 l, float3 v, float3 n, float3 base_color, float metalness, float roughness) {
	float n_l = dot(n, l);
	float n_v = dot(n, v);
	if (n_l < 0.0f || n_v < 0.0f) {
		return 0.0f;
	}

	float3 h = normalize(l + v);
	float n_h = dot(n, h);
	float l_h = dot(n, l);

	float3 specular0 = lerp((float3)0.08f, base_color, metalness);

	// diffuse Fresnel
	float fl = schlick_fresnel(n_l);
	float fv = schlick_fresnel(n_v);
	float f_diffuse90 = 0.5f + 2.0f * sqr(l_h) * roughness;
	float f_diffuse = lerp(1.0f, f_diffuse90, fl) * lerp(1.0f, f_diffuse90, fv);

	// specular
	float a = max(0.001f, sqr(roughness));
	float d = gtr2(n_h, a);
	float fh = schlick_fresnel(l_h);
	float3 f_specular = lerp(specular0, (float3)1.0f, fh);
	float gs = smith_g_ggx(n_l, a) * smith_g_ggx(n_v, a);

	return
		base_color * (f_diffuse * (1.0f - metalness) / pi) +
		gs * f_specular * d;
}

float3 disney_diffuse_brdf_times_pi(
	float3 l, float3 v, float3 n, float3 base_color, float metalness, float roughness
) {
	float n_l = dot(n, l);
	float n_v = dot(n, v);
	float3 h = normalize(l + v);
	float l_h = dot(l, h);

	// diffuse Fresnel
	float fl = schlick_fresnel(n_l);
	float fv = schlick_fresnel(n_v);
	float f_diffuse90 = 0.5f + 2.0f * sqr(l_h) * roughness;
	float f_diffuse = lerp(1.0f, f_diffuse90, fl) * lerp(1.0f, f_diffuse90, fv);

	return base_color * (f_diffuse * (1.0f - metalness));
}

float3 disney_specular_brdf(
	float3 l, float3 v, float3 n, float3 base_color, float metalness, float roughness
) {
	float n_l = dot(n, l);
	float n_v = dot(n, v);
	float3 h = normalize(l + v);
	float n_h = dot(n, h);
	float l_h = dot(l, h);

	// specular
	float a = max(0.001f, sqr(roughness));
	float d = gtr2(n_h, a);
	float fh = schlick_fresnel(l_h);
	float3 specular0 = lerp((float3)0.08f, base_color, metalness);
	float3 f_specular = lerp(specular0, (float3)1.0f, fh);
	float gs = smith_g_ggx(n_l, a) * smith_g_ggx(n_v, a);

	return gs * f_specular * d;
}

// https://schuttejoe.github.io/post/ggximportancesamplingpart1/
float2 importance_sample_ggx_cos_theta(float alpha, float xi) {
	float alpha2 = sqr(alpha);
	float sqr_cos_theta = (1.0f - xi) / (xi * (alpha2 - 1.0f) + 1.0f);
	float cos_theta = sqrt(sqr_cos_theta);
	float inv_pdf = (pi * sqr((alpha2 - 1.0f) * sqr_cos_theta + 1.0f)) / (alpha2 * cos_theta);
	return float2(cos_theta, inv_pdf);
}

// https://hal.archives-ouvertes.fr/hal-01509746/document
// https://schuttejoe.github.io/post/ggximportancesamplingpart2/
float3 importance_sample_visible_ggx_smith_g(float3 v_orig, float roughness, float2 xi) {
	// stretch view
	float3 v = normalize(float3(v_orig.x * roughness, v_orig.y, v_orig.z * roughness));

	float3 t1 = v.y < 0.9999f ? normalize(cross(v, float3(0.0f, 1.0f, 0.0f))) : float3(1.0f, 0.0f, 0.0f);
	float3 t2 = cross(t1, v);

	// sample point in tangent plane
	float inv_a = 1.0f + v.y;
	float a = rcp(inv_a);
	float r = sqrt(xi.x);
	float phi = xi.y < a ? xi.y * inv_a * pi : (pi + pi * (xi.y - a) / (1.0f - a));
	float p1 = r * cos(phi);
	float p2 = r * sin(phi) * (xi.y < a ? 1.0f : v.y);

	// compute normal by projecting the point onto the hemisphere
	float3 n = p1 * t1 + p2 * t2 + sqrt(max(0.0f, 1.0f - sqr(p1) - sqr(p2))) * v;

	// unstretch
	n = normalize(float3(n.x * roughness, max(0.0f, n.y), n.z * roughness));
	return n;
}

#endif
