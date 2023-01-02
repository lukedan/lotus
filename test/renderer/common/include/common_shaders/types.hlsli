#ifndef COMMON_TYPES_HLSLI
#define COMMON_TYPES_HLSLI

struct geometry_data {
	uint index_buffer; // max_uint_v indicates `no index buffer'
	uint vertex_buffer;
	uint normal_buffer;
	uint tangent_buffer;
	uint uv_buffer;
};
struct rt_instance_data {
	float4x4 normal_transform;
	uint geometry_index;
	uint material_index;
};

#endif
