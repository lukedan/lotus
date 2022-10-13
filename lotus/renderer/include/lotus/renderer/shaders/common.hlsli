#ifndef LOTUS_RENDERER_COMMON_HLSLI
#define LOTUS_RENDERER_COMMON_HLSLI

static const float pi = 3.141592653589793;

float squared(float x) {
	return x * x;
}

#define LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(SPACE)    \
	SamplerState linear_sampler : register(s0, SPACE); \

#endif
