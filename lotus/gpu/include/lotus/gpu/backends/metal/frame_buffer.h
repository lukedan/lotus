#pragma once

/// \file
/// Metal frame buffers.

#include <cstddef>

#include "details.h"

namespace lotus::gpu::backends::metal {
	/// Holds the next \p CA::MetalDrawable.
	class swap_chain {
	protected:
		/// Initializes this object to empty.
		swap_chain(std::nullptr_t) {
		}

		[[nodiscard]] std::uint32_t get_image_count() const; // TODO
		[[nodiscard]] image2d get_image(std::uint32_t index) const; // TODO
		void update_synchronization_primitives(std::span<const back_buffer_synchronization>); // TODO

		/// Returns whether the object is valid.
		[[nodiscard]] bool is_valid() const {
			return _drawable;
		}
	private:
		CA::MetalDrawable *_drawable = nullptr; ///< The next back buffer to render onto.
	};

	// TODO
	class frame_buffer {
	protected:
		frame_buffer(std::nullptr_t); // TODO
	};
}
