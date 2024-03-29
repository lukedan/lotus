#ifndef LOTUS_RENDERER_GENERIC_PBR_MATERIAL_HLSLI
#define LOTUS_RENDERER_GENERIC_PBR_MATERIAL_HLSLI

#include "common.hlsli"
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
	float4 prev_position  : TEXCOORD0;
	float2 uv             : TEXCOORD1;
	float3 normal         : TEXCOORD2;
	float3 tangent        : TEXCOORD3;
	float  bitangent_sign : TEXCOORD4;
};

Texture2D<float4> material_textures[] : register(t0, space0);

ConstantBuffer<generic_pbr_material::material> instance_material : register(b0, space1);
ConstantBuffer<instance_data>                  instance          : register(b1, space1);
ConstantBuffer<view_data>                      view              : register(b2, space1);

LOTUS_DECLARE_BASIC_SAMPLER_BINDINGS(space2);

vs_output transform_geometry(vs_input input) {
	vs_output result = (vs_output)0;

	result.position      = mul(view.jittered_projection_view, mul(instance.transform,      float4(input.position, 1.0f)));
	result.prev_position = mul(view.prev_projection_view,     mul(instance.prev_transform, float4(input.position, 1.0f)));

#ifdef VERTEX_INPUT_HAS_UV
	result.uv = input.uv;
#else
	result.uv = float2(0.0f, 0.0f);
#endif

#ifdef VERTEX_INPUT_HAS_NORMAL
	result.normal = mul((float3x3)instance.normal_transform, input.normal);
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

material::basic_properties evaluate_material(vs_output input) {
	float4 albedo_sample = material_textures[instance_material.assets.albedo_texture    ].Sample(linear_sampler, input.uv);
	float4 normal_sample = material_textures[instance_material.assets.normal_texture    ].Sample(linear_sampler, input.uv);
	float4 props_sample  = material_textures[instance_material.assets.properties_texture].Sample(linear_sampler, input.uv);

	material::basic_properties result = (material::basic_properties)0;
	result.albedo      = albedo_sample.rgb * instance_material.properties.albedo_multiplier.rgb;
#ifdef MATERIAL_IS_MASKED
	result.presence    = albedo_sample.a < instance_material.properties.alpha_cutoff ? 0.0f : 1.0f;
#else
	result.presence    = 1.0f;
#endif
	result.normal_ts   = normal_sample.rgb * 2.0f - 1.0f;
	result.normal_ts.z = sqrt(1.0f - dot(result.normal_ts.xy, result.normal_ts.xy));
	result.glossiness  = 1.0f - props_sample.g * instance_material.properties.roughness_multiplier;
	result.metalness   = props_sample.b * instance_material.properties.metalness_multiplier;

	return result;
}

#endif
