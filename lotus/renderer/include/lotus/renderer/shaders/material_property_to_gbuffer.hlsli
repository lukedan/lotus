#ifndef MATERIAL_PROPERTIES_TO_GBUFFER_HLSLI
#define MATERIAL_PROPERTIES_TO_GBUFFER_HLSLI

#include "material_common.hlsli"

struct ps_output {
	float4 albedo_glossiness : SV_Target0;
	float3 normal            : SV_Target1;
	float  metalness         : SV_Target2;
};

ps_output material_to_gbuffer(material_output material) {
	ps_output result = (ps_output)0;
	result.albedo_glossiness = float4(material.albedo, material.glossiness);
	result.normal            = material.normal;
	result.metalness         = material.metalness;
	return result;
}

#endif
