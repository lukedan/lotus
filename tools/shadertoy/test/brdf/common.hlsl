#ifndef __COMMON_HLSL__
#define __COMMON_HLSL__

const static uint invalid_object = 0xFFFFFFFF;
struct distance_field {
	uint object_index;
	float min_distance;
};
struct raymarch_result {
	uint object_index;
	float distance;
	float min_cone_proportion;
	float3 position;
};
raymarch_result empty_raymarch_result() {
	raymarch_result result;
	result.object_index = invalid_object;
	result.distance = 0.0f;
	result.min_cone_proportion = 100000.0f;
	result.position = float3(0.0f, 0.0f, 0.0f);
	return result;
}

distance_field empty_distance_field() {
	distance_field result;
	result.object_index = invalid_object;
	result.min_distance = 1000000000000;
	return result;
}
void update_distance_field(float d, int obj, inout distance_field result) {
	if (d < result.min_distance) {
		result.min_distance = d;
		result.object_index = obj;
	}
}

void sphere(float3 pos, float3 center, float radius, int object_index, inout distance_field dist) {
	float d = length(pos - center) - radius;
	update_distance_field(d, object_index, dist);
}
void plane(float3 pos, float3 pt, float3 normal, int object_index, inout distance_field dist) {
	float d = dot(pos - pt, normalize(normal));
	update_distance_field(d, object_index, dist);
}


struct material {
	float3 albedo;
	float glossiness;
	float metalness;
	float3 emissive;
	float clearcoat;
	float clearcoat_gloss;
};


struct camera {
	float3 position;
	float3 front;
	float3 right;
	float3 up;
};

camera create_lookat_camera(
	float3 cam_pos, float3 lookat, float3 world_up, float tan_half_fovy, float aspect_ratio
) {
	camera result;
	result.position = cam_pos;
	result.front = normalize(lookat - cam_pos);
	result.right = tan_half_fovy * normalize(cross(result.front, world_up));
	result.up = cross(result.right, result.front);
	result.right *= aspect_ratio;
	return result;
}

float3 camera_ray(camera cam, float2 uv) {
	return cam.front + cam.right * (uv.x - 0.5f) + cam.up * -(uv.y - 0.5f);
}


// https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf
const static float pi = 3.14159265358979323846f;

float sqr(float x) {
	return x * x;
}

float schlick_fresnel(float u) {
	float m = saturate(1 - u);
	float m2 = m * m;
	return m2 * m2 * m;
}

float gtr1(float n_dot_h, float a) {
	if (a >= 1) {
		return 1 / pi;
	}
	float a2 = sqr(a);
	float t = 1 + (a2 - 1) * sqr(n_dot_h);
	return (a2 - 1) / (pi * log(a2) * t);
}

float gtr2(float n_dot_h, float a) {
	float a2 = sqr(a);
	float t = 1 + (a2 - 1) * sqr(n_dot_h);
	return a2 / (pi * sqr(t));
}

float gtr2_anisotropic(float n_dot_h, float h_dot_x, float h_dot_y, float ax, float ay) {
	return 1 / (pi * ax * ay * sqr(sqr(h_dot_x / ax) + sqr(h_dot_y / ay) + sqr(n_dot_h)));
}

float smith_g_ggx(float n_dot_v, float alpha_g) {
	float a = sqr(alpha_g);
	float b = sqr(n_dot_v);
	return 1 / (n_dot_v + sqrt(a + b - a * b));
}

float smith_g_ggx_anisotropic(float n_dot_v, float v_dot_x, float v_dot_y, float ax, float ay) {
	return 1 / (n_dot_v + sqrt(sqr(v_dot_x * ax) + sqr(v_dot_y * ay) + sqr(n_dot_v)));
}

float3 srgb_to_linear(float3 x) {
	return float3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

/*float3 disney_brdf_anisotropic(float3 L, float3 V, float3 N, float3 X, float3 Y) {
	float n_dot_l = dot(N, L);
	float n_dot_v = dot(N, V);
	if (n_dot_l < 0 || n_dot_v < 0) {
		return float3(0.0f, 0.0f, 0.0f);
	}

	float3 H = normalize(L + V);
	float NdotH = dot(N, H);
	float LdotH = dot(L, H);

	float3 Cdlin = mon2lin(baseColor);
	float Cdlum = .3 * Cdlin[0] + .6 * Cdlin[1] + .1 * Cdlin[2]; // luminance approx.

	float3 Ctint = Cdlum > 0 ? Cdlin / Cdlum : float3(1); // normalize lum. to isolate hue+sat
	float3 Cspec0 = lerp(specular * .08 * lerp(float3(1), Ctint, specularTint), Cdlin, metallic);
	float3 Csheen = lerp(float3(1), Ctint, sheenTint);

	// Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
	// and mix in diffuse retro-reflection based on roughness
	float FL = schlick_fresnel(n_dot_l), FV = schlick_fresnel(n_dot_v);
	float Fd90 = 0.5 + 2 * LdotH * LdotH * roughness;
	float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);

	// Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
	// 1.25 scale is used to (roughly) preserve albedo
	// Fss90 used to "flatten" retroreflection based on roughness
	float Fss90 = LdotH * LdotH * roughness;
	float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
	float ss = 1.25 * (Fss * (1 / (n_dot_l + n_dot_v) - .5) + .5);

	// specular
	float aspect = sqrt(1 - anisotropic * .9);
	float ax = max(.001, sqr(roughness) / aspect);
	float ay = max(.001, sqr(roughness) * aspect);
	float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
	float FH = schlick_fresnel(LdotH);
	float3 Fs = lerp(Cspec0, float3(1), FH);
	float Gs;
	Gs = smith_g_ggx_aniso(n_dot_l, dot(L, X), dot(L, Y), ax, ay);
	Gs *= smith_g_ggx_aniso(n_dot_v, dot(V, X), dot(V, Y), ax, ay);

	// sheen
	float3 Fsheen = FH * sheen * Csheen;

	// clearcoat (ior = 1.5 -> F0 = 0.04)
	float Dr = gtr1(NdotH, lerp(.1, .001, clearcoatGloss));
	float Fr = lerp(.04, 1.0, FH);
	float Gr = smith_g_ggx(n_dot_l, .25) * smith_g_ggx(n_dot_v, .25);

	return ((1 / pi) * lerp(Fd, ss, subsurface) * Cdlin + Fsheen)
		* (1 - metallic)
		+ Gs * Fs * Ds + .25 * clearcoat * Gr * Fr * Dr;
}*/

float3 disney_brdf(float3 L, float3 V, float3 N, material mat) {
	float roughness = 1.0f - mat.glossiness;

	float n_dot_l = dot(N, L);
	float n_dot_v = dot(N, V);
	if (n_dot_l < 0.0f || n_dot_v < 0.0f) {
		return float3(0.0f, 0.0f, 0.0f);
	}

	float3 H = normalize(L + V);
	float NdotH = dot(N, H);
	float LdotH = dot(L, H);

	float3 Cdlin = srgb_to_linear(mat.albedo);
	float Cdlum = .3 * Cdlin[0] + .6 * Cdlin[1] + .1 * Cdlin[2]; // luminance approx.

	float3 Ctint = Cdlum > 0 ? Cdlin / Cdlum : float3(1.0f, 1.0f, 1.0f); // normalize lum. to isolate hue+sat
	float3 Cspec0 = lerp(mat.albedo * .08 * float3(1.0f, 1.0f, 1.0f), Cdlin, mat.metalness);
	/*float3 Csheen = lerp(float3(1), Ctint, sheenTint);*/

	// Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
	// and mix in diffuse retro-reflection based on roughness
	float FL = schlick_fresnel(n_dot_l), FV = schlick_fresnel(n_dot_v);
	float Fd90 = 0.5 + 2 * LdotH * LdotH * roughness;
	float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);

	// Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
	// 1.25 scale is used to (roughly) preserve albedo
	// Fss90 used to "flatten" retroreflection based on roughness
	float Fss90 = LdotH * LdotH * roughness;
	float Fss = lerp(1.0, Fss90, FL) * lerp(1.0, Fss90, FV);
	float ss = 1.25 * (Fss * (1 / (n_dot_l + n_dot_v) - .5) + .5);

	// specular
	float a = max(.001, sqr(roughness));
	float Ds = gtr2(NdotH, a);
	float FH = schlick_fresnel(LdotH);
	float3 Fs = lerp(Cspec0, float3(1.0f, 1.0f, 1.0f), FH);
	float Gs;
	Gs = smith_g_ggx(n_dot_l, a);
	Gs *= smith_g_ggx(n_dot_v, a);

	// sheen
	/*float3 Fsheen = FH * sheen * Csheen;*/

	// clearcoat (ior = 1.5 -> F0 = 0.04)
	float Dr = gtr1(NdotH, lerp(.1, .001, mat.clearcoat_gloss));
	float Fr = lerp(.04, 1.0, FH);
	float Gr = smith_g_ggx(n_dot_l, .25) * smith_g_ggx(n_dot_v, .25);

	return ((1 / pi) * /*lerp(Fd, ss, subsurface)*/Fd * Cdlin/* + Fsheen*/)
		* (1 - mat.metalness)
		+ Gs * Fs * Ds + .25 * mat.clearcoat * Gr * Fr * Dr;
}

#endif // __COMMON_HLSL__
