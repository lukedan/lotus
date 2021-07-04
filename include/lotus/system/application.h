#pragma once

/// \file
/// An application class used to create windows and interface with the system.

#include <string_view>

#include LOTUS_SYSTEM_PLATFORM_INCLUDE

namespace lotus::system {
	/// Interface to the operating system and windowing system.
	class application : public platform::application {
	public:
		/// Initializes the application.
		explicit application(std::u8string_view name) : platform::application(name) {
		}

		/// Creates a new window.
		[[nodiscard]] window create_window() {
			return platform::application::create_window();
		}
		/// Waits for and processes one message.
		///
		/// \return Whether the application should quit.
		bool process_message_blocking() {
			return platform::application::process_message_blocking();
		}
	};
}
