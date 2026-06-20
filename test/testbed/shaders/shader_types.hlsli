#ifndef SHADER_TYPES_HLSLI
#define SHADER_TYPES_HLSLI

struct default_shader_constants {
	float4x4 projection_view;
};

struct point_shader_constants {
	float4x4 projection_view;
	float4x4 view;
	float3 down;
	float _pad;
	float3 right;
	float opacity_multiplier;
};

struct line_shader_constants {
	float4x4 projection_view;
	float opacity_multiplier;
};

struct shadow_constants {
	float4x4 projection_view;
};

struct light_constants {
	float3 light_direction;
	float _pad;
	float3 light_color;
};

struct ssao_constants {
	float4x4 inv_projection;

	uint2 image_size;
	float2 rcp_image_size;

	uint angular_samples;
	uint radial_samples;
	float2 depth_linearize_mul_add;

	float radius_pixels_1m;
	float radius_ws;
	float smoothing;
};

struct sky_constants {
	float3 color;
};

#endif
