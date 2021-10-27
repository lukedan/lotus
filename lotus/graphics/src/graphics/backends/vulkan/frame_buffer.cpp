#include "lotus/graphics/backends/vulkan/frame_buffer.h"

/// \file
/// Implementation of Vulkan frame buffers and swap chains.

namespace lotus::graphics::backends::vulkan {
	image2d swap_chain::get_image(std::size_t index) {
		image2d result = nullptr;
		result._image = _images[index];
		return result;
	}

	void swap_chain::update_synchronization_primitives(std::span<const back_buffer_synchronization> prims) {
		assert(prims.size() == _synchronization.size());
		for (std::size_t i = 0; i < prims.size(); ++i) {
			_synchronization[i].next_fence = prims[i].notify_fence;
		}
	}
}
