#pragma once

/// \file
/// Application class for Windows.

#include <string_view>

#include "lotus/system/common.h"
#include "window.h"

namespace lotus::system::platforms::windows {
	/// Holds the registered window class.
	class application {
	protected:
		/// Initializes this application with the given application name.
		explicit application(std::u8string_view);
		/// Unregisters the window class.
		~application();

		/// Creates a Win32 window.
		[[nodiscard]] window create_window() const;

		/// Processes a message using \p GetMessage().
		message_type process_message_blocking();
		/// Processes a message if one exists, using \p PeekMessage().
		message_type process_message_nonblocking();

		/// Calls \p PostQuitMessage().
		void quit();
	private:
		ATOM _window_class = 0; ///< The window class.
	};
}
