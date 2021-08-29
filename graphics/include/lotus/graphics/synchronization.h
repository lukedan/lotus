#pragma once

/// \file
/// Generic interface for synchronization primitives.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class device;


	/// A synchronization primitive used to notify the CPU of an GPU event.
	class fence : public backend::fence {
		friend device;
	public:
		/// Creates an empty fence.
		fence(std::nullptr_t) : backend::fence(nullptr) {
		}
		/// Move constructor.
		fence(fence &&src) : backend::fence(std::move(src)) {
		}
		/// No copy construction.
		fence(const fence&) = delete;
		/// Move assignment.
		fence &operator=(fence &&src) {
			backend::fence::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		fence &operator=(const fence&) = delete;
	protected:
		/// Initializes the base class.
		fence(backend::fence base) : backend::fence(std::move(base)) {
		}
	};
}
