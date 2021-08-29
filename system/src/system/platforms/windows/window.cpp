#include "lotus/system/platforms/windows/window.h"

/// \file
/// Implementation of windows on Windows.

#include <cstdint>

namespace lotus::system::platforms::windows {
	window::~window() {

	}

	void window::show() {
		ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
	}

	void window::show_and_activate() {
		ShowWindow(_hwnd, SW_SHOW);
	}

	void window::hide() {
		ShowWindow(_hwnd, SW_HIDE);
	}

	window::window(HWND hwnd) : _hwnd(hwnd) {
	}
}
