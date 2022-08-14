#ifndef LOTUS_RENDERER_GLTF_MATERIAL_HLSLI
#define LOTUS_RENDERER_GLTF_MATERIAL_HLSLI

#include "types.hlsli"

struct vs_input {
	float3 position : POSITION;
#ifdef VERTEX_INPUT_HAS_UV
	float2 uv       : TEXCOORD;
#endif
#ifdef VERTEX_INPUT_HAS_NORMAL
	float3 normal   : NORMAL;
#endif
#ifdef VERTEX_INPUT_HAS_TANGENT
	float4 tangent  : TANGENT;
#endif
};

struct vs_output {
	float4 position       : SV_Position;
	float2 uv             : TEXCOORD0;
	float3 normal         : TEXCOORD1;
	float3 tangent        : TEXCOORD2;
	float  bitangent_sign : TEXCOORD3;
};

Texture2D<float4> material_textures[] : register(t0, space0);

ConstantBuffer<gltf_material> material : register(b0, space1);
ConstantBuffer<instance_data> instance : register(b1, space1);
ConstantBuffer<view_data> view         : register(b2, space1);
SamplerState linear_sampler            : register(s3, space1);

vs_output transform_geometry(vs_input input) {
	vs_output result = (vs_output)0;
	result.position = mul(view.projection_view, mul(instance.transform, float4(input.position, 1.0f)));
#ifdef VERTEX_INPUT_HAS_UV
	result.uv = input.uv;
#else
	result.uv = float2(0.0f, 0.0f);
#endif
#ifdef VERTEX_INPUT_HAS_NORMAL
	result.normal = mul((float3x3)instance.transform, input.normal);
#else
	result.normal = float3(0.0f, 1.0f, 0.0f);
#endif
#ifdef VERTEX_INPUT_HAS_TANGENT
	result.tangent        = mul((float3x3)instance.transform, input.tangent.xyz);
	result.bitangent_sign = input.tangent.w;
#else
	result.tangent        = float3(0.0f, 0.0f, 1.0f);
	result.bitangent_sign = 1.0f;
#endif
	return result;
}

material_output evaluate_material(vs_output input) {
	material_output result = (material_output)0;
	result.albedo    = material_textures[material.assets.albedo_texture].Sample(linear_sampler, input.uv);
	result.normal_ts = material_textures[material.assets.normal_texture].Sample(linear_sampler, input.uv) * 2.0f - 1.0f;
	return result;
}

#endif
