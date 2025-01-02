#pragma once

/// \file
/// Metal frame buffers.

#include <cstddef>

#include <QuartzCore/QuartzCore.hpp>

#include "details.h"
#include "resources.h"

namespace lotus::gpu::backends::metal {
	class context;
	class device;

	/// Holds the next \p CA::MetalDrawable.
	class swap_chain {
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
		_details::metal_ptr<CA::MetalDrawable> _drawable; ///< The next back buffer to render onto.
	};

	// TODO
	class frame_buffer {
	protected:
		frame_buffer(std::nullptr_t); // TODO
	};
}
