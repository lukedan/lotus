#include "material_common.hlsli"
#include LOTUS_MATERIAL_INCLUDE

struct ps_output {
	float4 albedo_glossiness : SV_Target0;
	float3 normal            : SV_Target1;
	float  metalness         : SV_Target2;
};

ps_output material_to_gbuffer(vs_output input, material_output material) {
	ps_output result = (ps_output)0;
	result.albedo_glossiness = float4(material.albedo, material.glossiness);
	result.normal            = evaluate_normal_mikkt(
		material.normal_ts, input.normal, input.tangent, input.bitangent_sign
	);
	result.metalness         = material.metalness;
	return result;
}

ps_output main_ps(vs_output input) {
	return material_to_gbuffer(input, evaluate_material(input));
}
