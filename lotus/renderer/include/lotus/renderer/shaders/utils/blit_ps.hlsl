#include "fullscreen_quad_vs.hlsl"

Texture2D<float4> image : register(t0);
SamplerState point_sampler : register(s1);

float4 main_ps(ps_input input) : SV_TARGET0 {
	return image.SampleLevel(point_sampler, input.uv, 0);
}
