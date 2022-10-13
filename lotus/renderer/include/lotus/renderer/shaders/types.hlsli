#ifndef LOTUS_RENDERER_TYPES_HLSLI
#define LOTUS_RENDERER_TYPES_HLSLI

struct instance_data {
	float4x4 transform;
};

struct view_data {
	float4x4 view;
	float4x4 projection;
	float4x4 projection_view;
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
		uint _padding;
	};

	struct material {
		material_properties properties;
		material_assets     assets;
	};
}

#endif
