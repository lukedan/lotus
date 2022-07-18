#ifndef __MISC_HLSL__
#define __MISC_HLSL__

const static float pi = 3.14159265f;

float sqr(float x) {
	return x * x;
}

// https://graphics.pixar.com/library/OrthonormalB/paper.pdf
void get_any_tangent_space(float3 n, out float3 tangent, out float3 bitangent) {
	float sign = n.z > 0.0f ? 1.0f : -1.0f;
	float a = -rcp(sign + n.z);
	float b = n.x * n.y * a;
	tangent   = float3(1.0f + sign * sqr(n.x) * a, sign * b,            -sign * n.x);
	bitangent = float3(b,                          sign * sqr(n.y) * a, -n.y       );
}

float2 specular_glossiness_to_metalness_roughness(float3 base_color, float3 specular, float glossiness) {
	const float specular_0 = 0.04f;
	float roughness = 1.0f - glossiness;
	float3 average = 0.5f * (base_color + specular);
	float3 s = sqrt(average * average - specular_0 * base_color);
	float3 metalness_rgb = (1.0f / specular_0) * average - s;
	float metalness = (1.0f / 3.0f) * (metalness_rgb.r + metalness_rgb.g + metalness_rgb.b);
	return float2(metalness, roughness);
}

#endif
