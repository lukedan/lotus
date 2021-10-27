#pragma once

/// \file
/// Vulkan synchronization primitives.

#include "details.h"

namespace lotus::graphics::backends::vulkan {
	class command_queue;
	class device;


	/// Contains a \p vk::UniqueFence.
	class fence {
		friend device;
		friend command_queue;
	protected:
		/// Creates an empty object.
		fence(std::nullptr_t) {
		}
	private:
		vk::UniqueFence _fence; ///< The fence.
	};
}
