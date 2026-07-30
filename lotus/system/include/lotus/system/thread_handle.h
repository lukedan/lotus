#pragma once

/// \file
/// Thread handles.

#include <algorithm>
#include <string>
#include <string_view>

#include LOTUS_SYSTEM_PLATFORM_INCLUDE_COMMON
#include LOTUS_SYSTEM_PLATFORM_INCLUDE_THREAD_HANDLE

namespace lotus::system {
	/// Handle of a running thread.
	struct thread_handle : public platform::thread_handle {
	public:
		using platform::thread_handle::native_handle_t;

		/// Initializes this object to empty.
		thread_handle(std::nullptr_t) : platform::thread_handle(nullptr) {
		}

		/// Sets the name of this thread.
		void set_name(std::u8string_view name) {
			platform::thread_handle::set_name(name);
		}
		/// Gets the name of this thread.
		[[nodiscard]] std::u8string get_name() const {
			return platform::thread_handle::get_name();
		}

		/// Returns a handle to the current thread.
		[[nodiscard]] static thread_handle current() {
			return platform::thread_handle::current();
		}

		/// Returns the native handle.
		[[nodiscard]] native_handle_t get_native_handle() const {
			return platform::thread_handle::get_native_handle();
		}
	protected:
		/// Initializes the base class.
		thread_handle(platform::thread_handle base) : platform::thread_handle(std::move(base)) {
		}
	};
}
