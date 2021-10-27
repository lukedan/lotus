#pragma once

/// \file
/// Window implementation on Windows.

#include <Windows.h>

#include "lotus/math/vector.h"

namespace lotus::graphics::backends::directx12 {
	class context;
}
namespace lotus::system::platforms::windows {
	class application;

	/// Window implementation on Windows.
	class window {
		friend graphics::backends::directx12::context;
		friend application;
		friend LRESULT CALLBACK _window_proc(HWND, UINT, WPARAM, LPARAM);
	public:
		/// Destroys the window.
		~window();
	protected:
		using native_handle_t = HWND; ///< Native handle type.

		/// Initializes \ref _hwnd to \p nullptr.
		window(std::nullptr_t) : _hwnd(nullptr) {
		}
		/// Calls \ref _update_address().
		window(window&&);
		/// No copy construction.
		window(const window&) = delete;
		/// No copy assignment.
		window &operator=(const window&) = delete;

		/// Shows the window using \p ShowWindow().
		void show();
		/// Shows and activates the window using \p ShowWindow().
		void show_and_activate();
		/// Hides the window without closing it using \p ShowWindow().
		void hide();

		/// Calls \p SetCapture() to acquire capture for this window.
		void acquire_mouse_capture();
		/// Calls \p ReleaseCapture().
		void release_mouse_capture();

		/// Returns the result of \p GetClientRect().
		[[nodiscard]] cvec2s get_size() const;

		/// Returns \ref _hwnd.
		[[nodiscard]] native_handle_t get_native_handle() const {
			return _hwnd;
		}
	private:
		native_handle_t _hwnd; ///< The window handle.

		/// Initializes \ref _hwnd and calls \ref _update_address().
		explicit window(native_handle_t);

		/// Updates the address stored with \p SetWindowLongPtr().
		void _update_address();
	};
}
