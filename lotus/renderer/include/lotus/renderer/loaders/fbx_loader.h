#pragma once

/// \file
/// FBX file loader.

#ifdef LOTUS_RENDERER_HAS_FBX

#include <memory>
#include <filesystem>

#include "lotus/utils/static_function.h"
#include "lotus/renderer/context/assets.h"
#include "lotus/renderer/context/asset_manager.h"
#include "lotus/renderer/generic_pbr_material.h"

namespace lotus::renderer::fbx {
	namespace _details {
		struct sdk;
	}

	/// Holds a handle to the FBX library.
	class context {
	public:
		/// Creates a new context object.
		[[nodiscard]] static context create(assets::manager&);
		/// Destructor.
		~context();

		/// Loads the specified FBX file.
		void load(
			const std::filesystem::path&,
			static_function<void(assets::handle<assets::image2d>)> image_loaded_callback,
			static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
			static_function<void(assets::handle<assets::material>)> material_loaded_callback,
			static_function<void(instance)> instance_loaded_callback,
			const pool &buf_pool, const pool &tex_pool
		);
	private:
		assets::manager &_asset_manager; ///< The asset manager.
		_details::sdk *_sdk = nullptr; ///< PImpl SDK pointer.

		/// Initializes this context.
		context(assets::manager &man, _details::sdk *sdk) : _asset_manager(man), _sdk(sdk) {
		}
	};

	using material_data = generic_pbr_material_data; ///< FBX uses generic PBR materials.
}

#endif // LOTUS_RENDERER_HAS_FBX
