#pragma once

/// \file
/// Window implementation on Windows.

#include <Windows.h>

namespace lotus::graphics::backends::directx12 {
	class context;
}
namespace lotus::system::platforms::windows {
	class application;

	/// Window implementation on Windows.
	class window {
		friend graphics::backends::directx12::context;
		friend application;
	public:
		/// Destroys the window.
		~window();
	protected:
		/// Shows the window using \p ShowWindow().
		void show();
		/// Shows and activates the window using \p ShowWindow().
		void show_and_activate();
		/// Hides the window without closing it using \p ShowWindow().
		void hide();
	private:
		HWND _hwnd = nullptr; ///< The window handle.

		/// Creates a new window with the given window handle.
		explicit window(HWND);
	};
}
