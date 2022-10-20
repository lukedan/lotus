#pragma once

/// \file
/// Generic single-layer PBR materials.

#include "context/assets.h"
#include "context/resource_bindings.h"
#include "shader_types.h"

namespace lotus::renderer {
	/// Generic single-layer PBR material parameters.
	class generic_pbr_material_data : public assets::material::context_data {
	public:
		/// Initializes this material to empty.
		explicit generic_pbr_material_data(assets::manager &man) :
			albedo_texture(nullptr), normal_texture(nullptr), properties_texture(nullptr), manager(&man) {
		}

		/// Returns "gltf_material.hlsli".
		[[nodiscard]] std::u8string_view get_material_include() const override {
			return u8"\"generic_pbr_material.hlsli\"";
		}
		/// Creates resource bindings for this material.
		[[nodiscard]] all_resource_bindings create_resource_bindings() const override;

		shader_types::generic_pbr_material::material_properties properties; ///< Properties of this material.
		assets::handle<assets::image2d> albedo_texture;     ///< Albedo texture.
		assets::handle<assets::image2d> normal_texture;     ///< Normal texture.
		assets::handle<assets::image2d> properties_texture; ///< Properties texture.
		assets::manager *manager = nullptr; ///< The associated asset manager.
	};
}
