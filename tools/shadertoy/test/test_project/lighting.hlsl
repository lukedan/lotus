#include "scene.hlsl"

Texture2D<float4> albedo_metalness;
Texture2D<float4> normal_glossiness;
Texture2D<float4> position_depth;
Texture2D<float3> emissive;
Texture2D<float2> clearcoat;
Texture2D<float> shadow;

float4 main(ps_input input) : SV_Target0 {
	float3 camera_pos = scene_camera_position();
	float3 light_pos = scene_light_position();

	float4 tex_albedo_metalness = albedo_metalness.Sample(nearest_sampler, input.uv);
	float4 tex_normal_glossiness = normal_glossiness.Sample(nearest_sampler, input.uv);
	float4 tex_position_depth = position_depth.Sample(nearest_sampler, input.uv);
	float3 tex_emissive = emissive.Sample(nearest_sampler, input.uv);
	float2 tex_clearcoat = clearcoat.Sample(nearest_sampler, input.uv);
	float tex_shadow = shadow.Sample(nearest_sampler, input.uv);

	float3 albedo = tex_albedo_metalness.xyz;
	float metalness = tex_albedo_metalness.w;
	float3 normal = tex_normal_glossiness.xyz;
	float glossiness = tex_normal_glossiness.w;
	float3 position = tex_position_depth.xyz;

	float3 pos_to_light = light_pos - position;
	float light_distance_sq = dot(pos_to_light, pos_to_light);
	float3 light_vec = normalize(pos_to_light);
	float3 view_vec = normalize(camera_pos - position);
	
	material mat;
	mat.albedo = albedo;
	mat.metalness = metalness;
	mat.glossiness = glossiness;
	mat.clearcoat = tex_clearcoat.x;
	mat.clearcoat_gloss = tex_clearcoat.y;
	float3 brdf = disney_brdf(light_vec, view_vec, normal, mat);
	float3 light = float3(20.0f, 20.0f, 20.0f) * tex_shadow;

	return float4(
		tex_emissive + light * brdf * (dot(light_vec, normal) / light_distance_sq), 1.0f
	);
}
