#ifndef TYPES_HLSLI
#define TYPES_HLSLI

struct global_data {
	float3 camera_position;
	float t_min;

	float3 top_left;
	float t_max;

	float3 right;
	float _padding;

	float3 down;
	uint frame_index;
};

struct geometry_data {
	uint index_buffer;
	uint vertex_buffer;
	uint normal_buffer;
	uint tangent_buffer;
	uint uv_buffer;
};
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
struct instance_data {
	float3x3 normal_transform;
	uint geometry_index;
	uint material_index;
};

#endif // TYPES_HLSLI
