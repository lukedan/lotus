#ifndef LOTUS_SHADER_TYPES_HLSLI
#define LOTUS_SHADER_TYPES_HLSLI

struct gltf_material_properties {
	float4 albedo_multiplier;
	float  normal_scale;
	float  metalness_multiplier;
	float  roughness_multiplier;
	float  alpha_cutoff;
};

struct gltf_material_assets {
	uint albedo_texture;
	uint normal_texture;
	uint properties_texture;
};

struct gltf_material {
	gltf_material_properties properties;
	gltf_material_assets assets;
};

#endif
