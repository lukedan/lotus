#pragma once

/// \file
/// Application class for MacOS.

#include "lotus/system/common.h"
#include "window.h"

namespace lotus::system::platforms::macos {
	namespace _details {
		/// Custom event type.
		enum class custom_event_type : short {
			quit, ///< Event indicating that the application should quit.
			window_initialized, ///< Event indicating that a window has just been created.
		};
	}

	/// Application class for MacOS.
	class application {
	protected:
		/// Initializes the application.
		explicit application(std::u8string_view);

		/// Creates a \p NSWindow.
		[[nodiscard]] window create_window() const;

		/// Processes a single message, blocking until one is received.
		message_type process_message_blocking();
		/// Processes a single message, returning immediately if none are present.
		message_type process_message_nonblocking();

		/// Calls <tt>NSApp terminate</tt>.
		void quit();
	};
}
