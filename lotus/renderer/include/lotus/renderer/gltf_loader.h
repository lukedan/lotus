#pragma once

/// \file
/// GLTF loader and utilities.

#include "asset_manager.h"
#include "context.h"

namespace lotus::renderer::gltf {
	/// GLTF context.
	class context {
	public:
		/// Creates a new context.
		[[nodiscard]] inline static context create(assets::manager &asset_man) {
			return context(asset_man);
		}

		/// Loads the given GLTF file.
		[[nodiscard]] std::vector<instance> load(const std::filesystem::path&);
	private:
		/// Initializes \ref _asset_manager and creates input resource layout.
		explicit context(assets::manager &asset_man) : _asset_manager(asset_man) {
		}

		assets::manager &_asset_manager; ///< Associated asset manager.
		assets::owning_handle<assets::shader> _vertex_shader = nullptr; ///< Vertex shader.
		assets::owning_handle<assets::shader> _pixel_shader = nullptr; ///< Pixel shader.
	};

	/// GLTF material parameters.
	class material_data : public assets::material::context_data {
	public:
		/// Initializes this material to empty.
		material_data(std::nullptr_t) :
			albedo_texture(nullptr), normal_texture(nullptr), properties_texture(nullptr) {
		}

		shader_types::gltf_material_properties properties; ///< Properties of this material.
		assets::owning_handle<assets::texture2d> albedo_texture;     ///< Albedo texture.
		assets::owning_handle<assets::texture2d> normal_texture;     ///< Normal texture.
		assets::owning_handle<assets::texture2d> properties_texture; ///< Properties texture.
	};
}
