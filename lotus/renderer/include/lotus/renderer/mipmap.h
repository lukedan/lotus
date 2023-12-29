#pragma once

/// \file
/// Utility class for generating mips.

#include "context/asset_manager.h"

namespace lotus::renderer::mipmap {
	/// Returns the maximum number of mip levels possible for an image of the given size.
	[[nodiscard]] inline constexpr std::uint32_t get_levels(cvec2u32 size) {
		return static_cast<std::uint32_t>(std::bit_width(std::max(size[0], size[1])));
	}
	/// Returns the size of a specified mip level.
	[[nodiscard]] inline constexpr cvec2u32 get_size(cvec2u32 top_mip_size, std::uint32_t mip_level) {
		return vec::memberwise_operation(
			[&](std::uint32_t s) {
				return std::max<std::uint32_t>(1, s >> mip_level);
			},
			top_mip_size
		);
	}

	/// Class that generates mipmaps for textures.
	struct generator {
	public:
		/// Creates a new context for the given asset manager.
		[[nodiscard]] static generator create(assets::manager&, context::queue);

		/// Inserts commands for generating all mip levels for the given image.
		void generate_all(image2d_view);
	private:
		/// Initializes all fields of this struct.
		generator(context::queue q, assets::handle<assets::shader> sh) : _shader(std::move(sh)), _q(q) {
		}

		assets::handle<assets::shader> _shader; ///< Shader used for generating mipmaps.
		context::queue _q; ///< Queue.
	};
}
