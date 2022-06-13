#ifndef GLOBAL_ASSETS_HLSLI
#define GLOBAL_ASSETS_HLSLI

SamplerState global_sampler_linear  : register(s0, space0);
Texture2D<float4> global_textures[] : register(t0, space0);

#endif
