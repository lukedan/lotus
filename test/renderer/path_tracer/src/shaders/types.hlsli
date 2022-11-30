#ifndef TYPES_HLSLI
#define TYPES_HLSLI

#include "common_shaders/types.hlsli"

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

#endif // TYPES_HLSLI
