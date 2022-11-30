#ifndef __MISC_HLSL__
#define __MISC_HLSL__

float sqr(float x) {
	return x * x;
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
