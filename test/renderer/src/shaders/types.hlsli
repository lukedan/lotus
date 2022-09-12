#ifndef TYPES_HLSLI
#define TYPES_HLSLI

struct material_data {
	float4 base_color;

	float normal_scale;
	float metalness;
	float roughness;
	float alpha_cutoff;

	uint base_color_index;
	uint normal_index;
	uint metallic_roughness_index;
	bool is_metallic_roughness;
};
struct global_data {
	float3 camera_position;
	float t_min;

	float3 top_left;
	float t_max;

	float3 right;
	float padding;

	float3 down;
	uint frame_index;
};
struct instance_data {
	uint first_index;
	uint first_vertex;
	uint material_index;
};

#endif // TYPES_HLSLI
