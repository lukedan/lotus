#ifndef SHADER_TYPES_HLSLI
#define SHADER_TYPES_HLSLI

struct default_shader_constants {
	float4x4 projection_view;
	float3 light_direction;
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

#endif
