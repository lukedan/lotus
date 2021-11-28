#ifndef __SCENE_HLSL__
#define __SCENE_HLSL__

#include "common.hlsl"

distance_field scene(float3 pos) {
	distance_field result = empty_distance_field();

	plane(pos, float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), 0, result);

	for (int x = -2; x <= 2; ++x) {
		for (int y = -2; y <= 2; ++y) {
			uint obj_id = 1 + (y + 2) * 5 + (x + 2);
			sphere(pos, float3(x * 1.5f, 0.5f, y * 1.5f), 0.5f, obj_id, result);
		}
	}

	return result;
}
float d_scene(float3 pos, float3 off) {
	return (scene(pos + off).min_distance - scene(pos - off).min_distance) / (2.0f * length(off));
}
float3 scene_normal(float3 pos, float delta) {
	return float3(
		d_scene(pos, float3(delta, 0.0f, 0.0f)),
		d_scene(pos, float3(0.0f, delta, 0.0f)),
		d_scene(pos, float3(0.0f, 0.0f, delta))
	);
}
raymarch_result raymarch_scene(float3 pos, float3 dir, uint steps, float radius = 0.0f) {
	float rcp_dir_length = rcp(length(dir));
	float3 ray = dir * rcp_dir_length;
	float sin_half_cone = radius * rcp_dir_length;

	raymarch_result res = empty_raymarch_result();
	res.position = pos;
	for (uint i = 0; i < steps; ++i) {
		distance_field step = scene(res.position);
		// need to update this before distance
		res.min_cone_proportion = min(
			res.min_cone_proportion,
			step.min_distance / (sin_half_cone * saturate(res.distance))
		);
		res.position += ray * step.min_distance;
		res.distance += step.min_distance;
		res.object_index = step.object_index;
	}
	res.distance *= rcp_dir_length;
	return res;
}

const static float3 row_albedo[5] = {
	float3(1.0f, 0.0f, 0.0f),
	float3(0.0f, 1.0f, 0.0f),
	float3(0.0f, 0.0f, 1.0f),
	float3(1.0f, 1.0f, 0.0f),
	float3(0.0f, 1.0f, 1.0f)
};
material scene_material(uint index, float3 pos) {
	material result;
	result.albedo = float3(0.6f, 0.6f, 0.6f);
	result.glossiness = 0.2;
	result.metalness = 0.0;
	result.emissive = float3(0.0f, 0.0f, 0.0f);
	result.clearcoat = 0.0f;
	result.clearcoat_gloss = 0.0f;
	if (index == 0) {
		return result;
	}
	uint row_x = (index - 1) / 5;
	uint row_y = (index - 1) % 5;
	result.albedo = row_albedo[row_y];
	if (row_y == 0) {
		result.glossiness = 0.25f * row_x;
	} else if (row_y == 1) {
		result.metalness = 1.0f;
		result.glossiness = 0.25f * row_x;
	} else if (row_y == 2) {
		result.glossiness = 0.6f;
		result.metalness = 0.25f * row_x;
	} else if (row_y == 3) {
		result.emissive = row_x * 0.1f * row_albedo[row_y];
	} else if (row_y == 4) {
		result.glossiness = 0.4f;
		result.clearcoat = 1.0f;
		result.clearcoat_gloss = 0.25f * row_x;
	}
	return result;
}

float3 scene_camera_position() {
	float2 mouse = (globals.mouse_drag + float2(100.0f, 100.0f)) * 0.01f;
	return 10.0f * float3(cos(mouse.y) * cos(mouse.x), sin(mouse.y), cos(mouse.y) * sin(mouse.x));
}
camera scene_camera() {
	return create_lookat_camera(
		scene_camera_position(), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f),
		1.0f, globals.resolution.x / (float)globals.resolution.y
	);
}
float3 scene_light_position() {
	return float3(5.0f * cos(globals.time * 1.7f), 4.0f + sin(globals.time), 5.0f * sin(globals.time * 1.9f));
}

#endif // __SCENE_HLSL__
