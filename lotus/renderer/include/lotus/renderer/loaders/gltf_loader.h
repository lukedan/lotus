#pragma once

/// \file
/// GLTF loader and utilities.

#include "lotus/renderer/asset_manager.h"
#include "lotus/renderer/context.h"
#include "lotus/renderer/shader_types.h"
#include "lotus/renderer/generic_pbr_material.h"

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
			static_function<void(assets::handle<assets::image2d>)> image_loaded_callback,
			static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
			static_function<void(assets::handle<assets::material>)> material_loaded_callback,
			static_function<void(instance)> instance_loaded_callback
		);
	private:
		assets::manager &_asset_manager; ///< Associated asset manager.
	};

	using material_data = generic_pbr_material_data; ///< GLTF uses generic PBR materials.
}
