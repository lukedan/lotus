#pragma once

/// \file
/// GLTF loader and utilities.

#include "asset_manager.h"
#include "context.h"
#include "shader_types.h"
#include "mipmap.h"

namespace lotus::renderer::gltf {
	/// GLTF context.
	class context {
	public:
		/// Initializes \ref _asset_manager.
		explicit context(assets::manager &asset_man) : _asset_manager(asset_man) {
		}

		/// Loads the given GLTF file.
		void load(
			const std::filesystem::path&,
			static_function<void(assets::handle<assets::texture2d>)> image_loaded_callback,
			static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
			static_function<void(assets::handle<assets::material>)> material_loaded_callback,
			static_function<void(instance)> instance_loaded_callback
		);
	private:
		assets::manager &_asset_manager; ///< Associated asset manager.
	};

	/// GLTF material parameters.
	class material_data : public assets::material::context_data {
	public:
		/// Initializes this material to empty.
		explicit material_data(assets::manager &man) :
			albedo_texture(nullptr), normal_texture(nullptr), properties_texture(nullptr), manager(&man) {
		}

		/// Returns "gltf_material.hlsli".
		[[nodiscard]] std::u8string_view get_material_include() const override {
			return u8"\"gltf_material.hlsli\"";
		}
		/// Creates resource bindings for this material.
		[[nodiscard]] all_resource_bindings create_resource_bindings() const override;

		shader_types::gltf_material_properties properties; ///< Properties of this material.
		assets::handle<assets::texture2d> albedo_texture;     ///< Albedo texture.
		assets::handle<assets::texture2d> normal_texture;     ///< Normal texture.
		assets::handle<assets::texture2d> properties_texture; ///< Properties texture.
		assets::manager *manager = nullptr; ///< The associated asset manager.
	};
}
