#pragma once

/// \file
/// Generic interface for synchronization primitives.

#include LOTUS_GPU_BACKEND_INCLUDE_COMMON
#include LOTUS_GPU_BACKEND_INCLUDE_SYNCHRONIZATION

namespace lotus::gpu {
	class device;


	/// A synchronization primitive used to notify the CPU of an GPU event.
	class fence : public backend::fence {
		friend device;
	public:
		/// Creates an empty fence.
		fence(std::nullptr_t) : backend::fence(nullptr) {
		}
		/// Move constructor.
		fence(fence &&src) noexcept : backend::fence(std::move(src)) {
		}
		/// No copy construction.
		fence(const fence&) = delete;
		/// Move assignment.
		fence &operator=(fence &&src) noexcept {
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

	/// A synchronization primitive with a monotonically increasing value.
	class timeline_semaphore : public backend::timeline_semaphore {
		friend device;
	public:
		using value_type = _details::timeline_semaphore_value_type; ///< Value type of the semaphores.

		/// Creates an empty event.
		timeline_semaphore(std::nullptr_t) : backend::timeline_semaphore(nullptr) {
		}
		/// Move constructor.
		timeline_semaphore(timeline_semaphore &&src) noexcept : backend::timeline_semaphore(std::move(src)) {
		}
		/// No copy construction.
		timeline_semaphore(const timeline_semaphore&) = delete;
		/// Move assignment.
		timeline_semaphore &operator=(timeline_semaphore &&src) noexcept {
			backend::timeline_semaphore::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		timeline_semaphore &operator=(const timeline_semaphore&) = delete;
	protected:
		/// Initializes the base class.
		timeline_semaphore(backend::timeline_semaphore base) : backend::timeline_semaphore(std::move(base)) {
		}
	};
}
