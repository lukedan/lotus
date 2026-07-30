#pragma once

/// \file
/// MacOS thread handles.

#include <cstddef>
#include <string>
#include <string_view>

namespace lotus::system::platforms::macos {
	/// Contains a \p NSThread.
	struct thread_handle {
	protected:
		using native_handle_t = void*; ///< Actually \p NSThread*.

		/// Initializes this handle to empty.
		thread_handle(std::nullptr_t) : _handle(nullptr) {
		}

		void set_name(std::u8string_view);
		[[nodiscard]] std::u8string get_name() const;

		/// Returns <tt>[NSThread currentThread]</tt>.
		[[nodiscard]] static thread_handle current();

		/// Returns \ref _handle.
		[[nodiscard]] native_handle_t get_native_handle() const {
			return _handle;
		}
	private:
		native_handle_t _handle; ///< Thread handle.

		/// Initializes \ref _handle.
		explicit thread_handle(native_handle_t h) : _handle(h) {
		}
	};
}
