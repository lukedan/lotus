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
struct rt_instance_data {
	float3x3 normal_transform;
	uint geometry_index;
	uint material_index;
};

#endif // TYPES_HLSLI
