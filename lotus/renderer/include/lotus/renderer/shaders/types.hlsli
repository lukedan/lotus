#ifndef LOTUS_RENDERER_TYPES_HLSLI
#define LOTUS_RENDERER_TYPES_HLSLI

struct instance_data {
	float4x4 transform;
	float4x4 normal_transform;
};

struct view_data {
	float4x4 view;
	float4x4 projection;
	float4x4 projection_view;
};

struct debug_draw_data {
	float4x4 projection;
};

struct dear_imgui_draw_data {
	float4x4 projection;
	// we haven't exposed scissor rects yet, so use this dirty hack for now
	float2 scissor_min;
	float2 scissor_max;
	bool uses_texture;
};


namespace generic_pbr_material {
	struct material_properties {
		float4 albedo_multiplier;
		float  normal_scale;
		float  metalness_multiplier;
		float  roughness_multiplier;
		float  alpha_cutoff;
	};

	struct material_assets {
		uint albedo_texture;
		uint normal_texture;
		uint properties_texture;
		uint properties2_texture;
	};

	struct material {
		material_properties properties;
		material_assets     assets;
	};
}


enum class light_type : uint {
	directional_light,
	point_light,
	spot_light,
};

struct light {
	light_type type;
	float3 position;
	float3 direction;
	float3 color;
};

#endif
