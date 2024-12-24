#pragma once

/// \file
/// Application class for MacOS.

#include "lotus/system/common.h"
#include "window.h"

namespace lotus::system::platforms::macos {
	/// Application class for MacOS.
	class application {
	public:
		/// Destroys the application.
		~application();
	protected:
		/// Initializes the application.
		explicit application(std::u8string_view);

		/// Creates a \p NSWindow and a \p CAMetalLayer.
		[[nodiscard]] window create_window() const;

		/// Processes a single message, blocking until one is received.
		message_type process_message_blocking();
		/// Processes a single message, returning immediately if none are present.
		message_type process_message_nonblocking();

		/// Calls <tt>NSApp terminate</tt>.
		void quit();
	private:
		void *_pool; ///< The \p NSAutoreleasePool.
	};
}
