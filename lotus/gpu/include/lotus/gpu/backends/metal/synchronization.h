#pragma once

/// \file
/// Metal synchronization primitives.

#include <cstddef>

#include "details.h"

namespace lotus::gpu::backends::metal {
	class command_queue;
	class device;

	/// Holds a \p MTL::SharedEvent.
	class fence {
		friend command_queue;
		friend device;
	protected:
		/// Initializes the object to empty.
		fence(std::nullptr_t) {
		}
	private:
		NS::SharedPtr<MTL::SharedEvent> _event; ///< The event object.

		/// Initializes \ref _event.
		explicit fence(NS::SharedPtr<MTL::SharedEvent> event) : _event(std::move(event)) {
		}
	};

	/// Holds a \p MTL::SharedEvent.
	class timeline_semaphore {
		friend command_queue;
		friend device;
	protected:
		/// Initializes this object to empty.
		timeline_semaphore(std::nullptr_t) {
		}
	private:
		NS::SharedPtr<MTL::SharedEvent> _event; ///< The event object.

		/// Initializes \ref _event.
		explicit timeline_semaphore(NS::SharedPtr<MTL::SharedEvent> event) : _event(std::move(event)) {
		}
	};
}
