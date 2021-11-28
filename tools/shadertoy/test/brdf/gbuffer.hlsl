#include "common.hlsl"
#include "scene.hlsl"

struct output {
	float4 albedo_metalness : SV_Target0;
	float4 normal_glossiness : SV_Target1;
	float4 position_depth : SV_Target2;
	float4 emissive : SV_Target3;
	float4 clearcoat : SV_Target4;
};

const static float normal_dx = 0.001f;

output main(ps_input input) {
	camera cam = scene_camera();
	float3 ray = camera_ray(cam, input.uv);

	raymarch_result hit = raymarch_scene(cam.position, ray, 100);
	float3 normal = scene_normal(hit.position, normal_dx);
	material mat = scene_material(hit.object_index, hit.position);

	output result;
	result.albedo_metalness = float4(mat.albedo, mat.metalness);
	result.normal_glossiness = float4(normal, mat.glossiness);
	result.position_depth = float4(hit.position, hit.distance);
	result.emissive = float4(mat.emissive, 0.0f);
	result.clearcoat = float4(mat.clearcoat, mat.clearcoat_gloss, 0.0f, 0.0f);
	return result;
}
