#ifndef MATERIAL_COMMON_HLSLI
#define MATERIAL_COMMON_HLSLI

#include "types.hlsli"

struct vs_input {
	float3 position : POSITION;
#ifdef VERTEX_INPUT_HAS_UV
	float3 uv       : TEXCOORD;
#endif
#ifdef VERTEX_INPUT_HAS_NORMAL
	float3 normal   : NORMAL;
#endif
#ifdef VERTEX_INPUT_HAS_TANGENT
	float4 tangent  : TANGENT;
#endif
};

struct material_output {
	float3 albedo;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	float  glossiness;
	float  metalness;
};

#endif
