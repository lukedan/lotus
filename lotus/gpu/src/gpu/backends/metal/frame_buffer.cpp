#include "lotus/gpu/backends/metal/frame_buffer.h"

/// \file
/// Metal frame buffer implementation.

#include <QuartzCore/QuartzCore.hpp>

#include "lotus/gpu/backends/metal/resources.h"

namespace lotus::gpu::backends::metal {
	image2d swap_chain::get_image(u32 index) const {
		// return the current texture regardless
		// TODO unify behavior
		return image2d({ NS::RetainPtr(_drawable->texture()), nullptr }); // TODO
	}

	void swap_chain::update_synchronization_primitives(std::span<const back_buffer_synchronization>) {
	}
}
