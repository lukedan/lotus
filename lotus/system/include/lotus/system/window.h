#pragma once

/// \file
/// Windows.

#include LOTUS_SYSTEM_PLATFORM_INCLUDE
#include "lotus/utils/event_closure.h"

namespace lotus::system {
	class application;

	/// Abstract interface of a window.
	class window : public platform::window {
		friend application;
	public:
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
		/// Explicitly releases mouse capture.
		void release_mouse_capture() {
			platform::window::release_mouse_capture();
		}

		/// Returns the size of this window's client area.
		[[nodiscard]] cvec2s get_size() const {
			return platform::window::get_size();
		}

		/// Event that will be triggered when the user attempts to close the window.
		event<window&, window_events::close_request&>::head_node on_close_request = nullptr;
		/// Event that will be triggered when the user resizes the window.
		event<window&, window_events::resize&>::head_node on_resize = nullptr;
		/// Event that will be triggered when mouse movement is detected over the window.
		event<window&, window_events::mouse::move&>::head_node on_mouse_move = nullptr;
		/// Event that will be triggered when a mouse button is pressed over the window.
		event<window&, window_events::mouse::button_down&>::head_node on_mouse_button_down = nullptr;
		/// Event that will be triggered when a mouse button is released over the window.
		event<window&, window_events::mouse::button_up&>::head_node on_mouse_button_up = nullptr;
		/// Event that will be triggered when mouse capture is broken externally.
		event<window&>::head_node on_capture_broken = nullptr;
	protected:
		/// Implicit conversion from a base window.
		window(platform::window base) : platform::window(std::move(base)) {
		}
	};
}
