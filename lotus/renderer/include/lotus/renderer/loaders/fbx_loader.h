#pragma once

/// \file
/// FBX file loader.

#ifdef LOTUS_RENDERER_HAS_FBX

#include <memory>
#include <filesystem>

#include "lotus/utils/static_function.h"
#include "lotus/renderer/assets.h"
#include "lotus/renderer/asset_manager.h"

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
			static_function<void(assets::handle<assets::texture2d>)> image_loaded_callback,
			static_function<void(assets::handle<assets::geometry>)> geometry_loaded_callback,
			static_function<void(assets::handle<assets::material>)> material_loaded_callback,
			static_function<void(instance)> instance_loaded_callback
		);
	private:
		assets::manager &_asset_manager; ///< The asset manager.
		_details::sdk *_sdk = nullptr; ///< PImpl SDK pointer.

		/// Initializes this context.
		context(assets::manager &man, _details::sdk *sdk) : _asset_manager(man), _sdk(sdk) {
		}
	};
}

#endif // LOTUS_RENDERER_HAS_FBX
