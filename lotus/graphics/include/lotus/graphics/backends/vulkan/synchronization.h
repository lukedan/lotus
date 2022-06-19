#pragma once

/// \file
/// Vulkan synchronization primitives.

#include "details.h"

namespace lotus::graphics::backends::vulkan {
	class command_queue;
	class device;


	/// Contains a \p vk::UniqueFence.
	class fence {
		friend command_queue;
		friend device;
	protected:
		/// Creates an empty object.
		fence(std::nullptr_t) {
		}
	private:
		vk::UniqueFence _fence; ///< The fence.
	};

	/// Contains a Vulkan timeline semaphore.
	class timeline_semaphore {
		friend command_queue;
		friend device;
	protected:
		/// Initializes this to an empty object.
		timeline_semaphore(std::nullptr_t) {
		}
	private:
		vk::UniqueSemaphore _semaphore; ///< The semaphore.
	};
}
