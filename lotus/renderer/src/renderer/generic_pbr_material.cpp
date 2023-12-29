#include "lotus/renderer/generic_pbr_material.h"

/// \file
/// Implementation of generic PBR materials.

#include "lotus/renderer/context/asset_manager.h"

namespace lotus::renderer {
	all_resource_bindings generic_pbr_material_data::create_resource_bindings(constant_uploader &uploader) const {
		shader_types::generic_pbr_material::material mat;
		mat.properties = properties;
		mat.assets.albedo_texture     = albedo_texture     ? albedo_texture->descriptor_index     : manager->get_null_image()->descriptor_index;
		mat.assets.normal_texture     = normal_texture     ? normal_texture->descriptor_index     : manager->get_null_image()->descriptor_index;
		mat.assets.properties_texture = properties_texture ? properties_texture->descriptor_index : manager->get_null_image()->descriptor_index;

		return all_resource_bindings(
			{
				{ 0, manager->get_images() },
				{ 1, {
					{ 0, uploader.upload(mat) },
				} },
				{ 2, manager->get_samplers() },
			},
			{}
		);
	}

	std::vector<
		std::pair<std::u8string_view, std::u8string_view>
	> generic_pbr_material_data::get_additional_ps_defines() const {
		auto result = context_data::get_additional_ps_defines();
		if (properties.alpha_cutoff > 0.0f) {
			result.emplace_back(u8"MATERIAL_IS_MASKED", u8"");
		}
		return result;
	}
}
