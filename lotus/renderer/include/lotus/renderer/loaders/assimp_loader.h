#pragma once

/// \file
/// Assimp loader.

#ifdef LOTUS_RENDERER_HAS_ASSIMP

#include "lotus/renderer/context/asset_manager.h"
#include "lotus/renderer/generic_pbr_material.h"

namespace lotus::renderer::assimp {
	/// Assimp context.
	class context {
	public:
		/// Initializes \ref _asset_manager.
		explicit context(assets::manager &man) : _asset_manager(man) {
		}

		/// Loads the given file.
		void load(
			const std::filesystem::path&,
			static_function<void(assets::handle<assets::image2d>)> image_loaded_callback,
			static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
			static_function<void(assets::handle<assets::material>)> material_loaded_callback,
			static_function<void(instance)> instance_loaded_callback,
			static_function<void(shader_types::light)> light_loaded_callback,
			const pool &buf_pool, const pool &tex_pool
		);
	private:
		assets::manager &_asset_manager; ///< Associated asset manager.
	};

	using material_data = generic_pbr_material_data; ///< Uses generic PBR materials.
}

#endif
