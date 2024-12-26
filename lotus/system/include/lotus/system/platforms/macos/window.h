#pragma once

/// \file
/// Window implementation on MacOS.

#include "lotus/math/vector.h"

namespace lotus::system {
	class window;
}

namespace lotus::system::platforms::macos {
	class application;

	/// Window implementation on MacOS.
	class window {
		friend application;
	public:
		/// Destroys the window.
		~window();
	protected:
		/// Native handle type.
		struct native_handle_t {
			void *window = nullptr; ///< The \p NSWindow.
			void *metal_layer = nullptr; ///< The \p CAMetalLayer.
		};

		/// Initializes this window to empty.
		window(nullptr_t) {
		}
		/// Move construction.
		window(window&&);
		/// No copy construction.
		window(const window&) = delete;
		/// No copy assignment.
		window &operator=(const window&) = delete;

		/// Calls <tt>NSWindow setIsVisible</tt>.
		void show();
		/// Calls <tt>NSWindow setIsVisible</tt> and <tt>NSWindow makeKeyAndOrderFront</tt>.
		void show_and_activate();
		/// Calls <tt>NSWindow setIsVisible</tt>.
		void hide();

		void acquire_mouse_capture(); // TODO
		[[nodiscard]] bool has_mouse_capture() const; // TODO
		void release_mouse_capture(); // TODO

		/// \return \p NSWindow.frame.size.
		[[nodiscard]] cvec2s get_size() const;

		/// Calls <tt>NSWindow setTitle</tt>.
		void set_title(std::u8string_view);

		/// Returns \ref _handle.
		[[nodiscard]] native_handle_t get_native_handle() const {
			return _handle;
		}
	private:
		native_handle_t _handle; ///< The window handle.
		void *_delegate = nullptr; ///< The delegate.
		void *_tracking_area = nullptr; ///< The \p NSTrackingArea.
		system::window **_window_ptr = nullptr; ///< Pointer in the delegate to the window.

		/// Initializes the window.
		explicit window();
	};
}
