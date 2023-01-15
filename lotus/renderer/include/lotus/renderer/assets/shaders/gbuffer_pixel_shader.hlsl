#include "material_common.hlsli"
#include LOTUS_MATERIAL_INCLUDE

struct ps_output {
	float4 albedo_glossiness : SV_Target0;
	float3 normal            : SV_Target1;
	float  metalness         : SV_Target2;
	float2 velocity          : SV_Target3;
};

ps_output material_to_gbuffer(vs_output input, material::basic_properties mat) {
	float3 normal = material::evaluate_normal_mikkt(
		mat.normal_ts, input.normal, input.tangent, input.bitangent_sign
	);

	float3 prev_pos = input.prev_position.xyz / input.prev_position.w;
	prev_pos.xy = float2(prev_pos.x, -prev_pos.y) * 0.5f + 0.5f;
	float2 cur_pos_xy = input.position.xy * view.rcp_viewport_size;
	float2 offset = prev_pos.xy - cur_pos_xy;

	ps_output result = (ps_output)0;
	result.albedo_glossiness = float4(mat.albedo, mat.glossiness);
	result.normal            = normal;
	result.metalness         = mat.metalness;
	result.velocity          = offset;
	return result;
}

ps_output main_ps(vs_output input) {
	material::basic_properties mat_props = evaluate_material(input);
	if (mat_props.presence <= 0.0f) {
		discard;
	}
	return material_to_gbuffer(input, mat_props);
}
