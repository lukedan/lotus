#ifndef GLTF_MATERIAL_HLSLI
#define GLTF_MATERIAL_HLSLI

#include "types.hlsli"

struct vs_output {
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float3 normal   : TEXCOORD1;
	float3 tangent  : TEXCOORD2;
};

Texture2D<float4> material_textures[] : register(t0, space0);

ConstantBuffer<gltf_material> material : register(b0, space1);
ConstantBuffer<instance_data> instance : register(b1, space1);
SamplerState linear_sampler            : register(s2, space1);

vs_output transform_geometry(vs_input input) {
	vs_output result = (vs_output)0;
	result.position = mul(instance.transform, float4(input.position, 1.0f));
#ifdef VERTEX_INPUT_HAS_UV
	result.uv       = input.uv;
#else
	result.uv       = float2(0.0f, 0.0f);
#endif
#ifdef VERTEX_INPUT_HAS_NORMAL
	result.normal   = input.normal;
#else
	result.normal   = float3(0.0f, 1.0f, 0.0f);
#endif
#ifdef VERTEX_INPUT_HAS_TANGENT
	result.tangent  = input.tangent;
#else
	result.tangent  = float3(0.0f, 0.0f, 1.0f);
#endif
	return result;
}

material_output evaluate_material(vs_output input) {
	material_output result = (material_output)0;
	result.albedo = material_textures[material.assets.albedo_texture].Sample(linear_sampler, input.uv);
	result.normal = material_textures[material.assets.normal_texture].Sample(linear_sampler, input.uv);
	return result;
}

#endif
