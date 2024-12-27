#pragma once

/// \file
/// Windows.

#include "lotus/math/vector.h"
#include "lotus/utils/static_function.h"

#include LOTUS_SYSTEM_PLATFORM_INCLUDE_COMMON
#include LOTUS_SYSTEM_PLATFORM_INCLUDE_WINDOW

namespace lotus::system {
	class application;

	/// Abstract interface of a window.
	class window : public platform::window {
		friend application;
	public:
		using platform::window::native_handle_t;

		/// No copy construction.
		window(const window&) = delete;
		/// No copy assignment.
		window &operator=(const window&) = delete;

		/// Shows the window.
		void show() {
			platform::window::show();
		}
		/// Shows and activates the window.
		void show_and_activate() {
			platform::window::show_and_activate();
		}
		/// Hides the window.
		void hide() {
			platform::window::hide();
		}

		/// Captures the mouse cursor.
		void acquire_mouse_capture() {
			platform::window::acquire_mouse_capture();
		}
		/// Returns whether this window currently captures the mouse.
		[[nodiscard]] bool has_mouse_capture() const {
			return platform::window::has_mouse_capture();
		}
		/// Explicitly releases mouse capture.
		void release_mouse_capture() {
			platform::window::release_mouse_capture();
		}

		/// Returns the size of this window's client area.
		[[nodiscard]] cvec2s get_size() const {
			return platform::window::get_size();
		}

		/// Sets the title of this window.
		void set_title(std::u8string_view title) {
			platform::window::set_title(title);
		}

		/// Returns the native handle of this window.
		[[nodiscard]] native_handle_t get_native_handle() const {
			return platform::window::get_native_handle();
		}

		/// Function that will be called when the user attempts to close the window.
		static_function<void(window_events::close_request&)> on_close_request = nullptr;
		/// Function that will be called when the user resizes the window.
		static_function<void(window_events::resize&)> on_resize = nullptr;
		/// Function that will be called when mouse movement is detected over the window.
		static_function<void(window_events::mouse::move&)> on_mouse_move = nullptr;
		/// Function that will be called when the mouse leaves this window.
		static_function<void()> on_mouse_leave = nullptr;
		/// Function that will be called when a mouse button is pressed over the window.
		static_function<void(window_events::mouse::button_down&)> on_mouse_button_down = nullptr;
		/// Function that will be called when a mouse button is released over the window.
		static_function<void(window_events::mouse::button_up&)> on_mouse_button_up = nullptr;
		/// Function that will be called on a mouse scroll event.
		static_function<void(window_events::mouse::scroll&)> on_mouse_scroll = nullptr;
		/// Function that will be called when mouse capture is broken externally.
		static_function<void()> on_capture_broken = nullptr;
		/// Function that will be called when a key is pressed.
		static_function<void(window_events::key_down&)> on_key_down = nullptr;
		/// Function that will be called when a key is released.
		static_function<void(window_events::key_up&)> on_key_up = nullptr;
		/// Function that will be called when text input is detected.
		static_function<void(window_events::text_input&)> on_text_input = nullptr;
	protected:
		/// Implicit conversion from a base window.
		window(platform::window base) : platform::window(std::move(base)) {
		}
	};
}
