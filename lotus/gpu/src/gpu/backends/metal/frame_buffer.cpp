#include "lotus/gpu/backends/metal/frame_buffer.h"

/// \file
/// Metal frame buffer implementation.

#include <QuartzCore/QuartzCore.hpp>

#include "lotus/gpu/backends/metal/resources.h"

namespace lotus::gpu::backends::metal {
	image2d swap_chain::get_image(std::uint32_t index) const {
		crash_if(!_drawable);
		crash_if(index != _drawable->drawableID());
		_drawable->texture();
		std::abort(); // TODO
	}

	void swap_chain::update_synchronization_primitives(std::span<const back_buffer_synchronization>) {
	}
}
