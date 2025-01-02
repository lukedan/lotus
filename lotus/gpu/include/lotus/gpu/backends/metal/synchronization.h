#pragma once

/// \file
/// Metal synchronization primitives.

#include <cstddef>

#include "details.h"

namespace lotus::gpu::backends::metal {
	class device;

	// TODO
	class fence {
	protected:
		fence(std::nullptr_t); // TODO
	};

	/// Holds a \p MTL::SharedEvent.
	class timeline_semaphore {
		friend device;
	protected:
		/// Initializes this object to empty.
		timeline_semaphore(std::nullptr_t) {
		}
	private:
		_details::metal_ptr<MTL::SharedEvent> _event; ///< The event object.

		/// Initializes \ref _event.
		explicit timeline_semaphore(_details::metal_ptr<MTL::SharedEvent> event) : _event(std::move(event)) {
		}
	};
}
