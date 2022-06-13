#include "types.hlsli"
#include "global_assets.hlsli"
#include "material_property_to_gbuffer.hlsli"

struct vs_output {
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
	float3 normal   : TEXCOORD1;
	float3 tangent  : TEXCOORD2;
};

ConstantBuffer<gltf_material> material : register(b0, space1);

vs_output main_vs(vs_input input) {
	vs_output result = (vs_output)0;
	return result;
}

material_output material_ps(vs_output input) {
	material_output result = (material_output)0;
	result.albedo = global_textures[material.assets.albedo_texture].Sample(global_sampler_linear, input.uv);
	result.normal = global_textures[material.assets.normal_texture].Sample(global_sampler_linear, input.uv);
	return result;
}

ps_output main_ps(vs_output input) {
	return material_to_gbuffer(material_ps(input));
}
