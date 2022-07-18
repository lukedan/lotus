#include "material_common.hlsli"
#include LOTUS_MATERIAL_INCLUDE

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

ps_output main_ps(vs_output input) {
	return material_to_gbuffer(evaluate_material(input));
}
