#pragma once

/// \file
/// Metal frame buffers.

#include <cstddef>

#include <QuartzCore/QuartzCore.hpp>

#include "details.h"
#include "resources.h"

namespace lotus::gpu::backends::metal {
	class command_list;
	class command_queue;
	class context;
	class device;

	/// Holds the next \p CA::MetalDrawable.
	class swap_chain {
		friend command_queue;
		friend context;
		friend device;
	protected:
		/// Initializes this object to empty.
		swap_chain(std::nullptr_t) {
		}

		/// Returns \p CAMetalLayer.maximumDrawableCount.
		[[nodiscard]] std::uint32_t get_image_count() const;
		/// Checks that the index corresponds to \ref _drawable, and returns it.
		[[nodiscard]] image2d get_image(std::uint32_t index) const;
		/// Does nothing.
		void update_synchronization_primitives(std::span<const back_buffer_synchronization>);

		/// Checks if this object is valid.
		[[nodiscard]] bool is_valid() const {
			return _layer;
		}
	private:
		CA::MetalLayer *_layer = nullptr; ///< The \p CAMetalLayer.
		NS::SharedPtr<CA::MetalDrawable> _drawable; ///< The next back buffer to render onto.
	};

	/// Contains references to \p MTL::Texture objects for color and depth-stencil render targets, as well as the
	/// size of the frame buffer.
	class frame_buffer {
		friend command_list;
		friend device;
	protected:
		/// Initializes the object to empty.
		frame_buffer(std::nullptr_t) {
		}
	private:
		std::vector<MTL::Texture*> _color_rts; ///< Color render targets.
		MTL::Texture *_depth_stencil_rt = nullptr; ///< The depth-stencil target.
		cvec2u32 _size = zero; ///< The size of the frame buffer.
	};
}
