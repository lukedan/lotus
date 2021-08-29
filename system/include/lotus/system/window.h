#pragma once

/// \file
/// Windows.

#include LOTUS_SYSTEM_PLATFORM_INCLUDE

namespace lotus::system {
	class application;

	/// Abstract interface of a window.
	class window : public platform::window {
		friend application;
	public:
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
	protected:
		/// Implicit conversion from a base window.
		window(platform::window base) : platform::window(std::move(base)) {
		}
	};
}
