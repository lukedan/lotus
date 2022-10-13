#include "lotus/renderer/generic_pbr_material.h"

/// \file
/// Implementation of generic PBR materials.

#include "lotus/renderer/asset_manager.h"

namespace lotus::renderer {
	all_resource_bindings generic_pbr_material_data::create_resource_bindings() const {
		shader_types::generic_pbr_material::material mat;
		mat.properties = properties;
		mat.assets.albedo_texture     = albedo_texture     ? albedo_texture->descriptor_index     : manager->get_invalid_image()->descriptor_index;
		mat.assets.normal_texture     = normal_texture     ? normal_texture->descriptor_index     : manager->get_invalid_image()->descriptor_index;
		mat.assets.properties_texture = properties_texture ? properties_texture->descriptor_index : manager->get_invalid_image()->descriptor_index;

		return all_resource_bindings::assume_sorted({
			resource_set_binding(manager->get_images(), 0),
			resource_set_binding::descriptors({
				resource_binding(descriptor_resource::immediate_constant_buffer::create_for(mat), 0),
			}).at_space(1),
			resource_set_binding(manager->get_samplers(), 2),
		});
	}
}
