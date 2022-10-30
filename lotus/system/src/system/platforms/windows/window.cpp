#include "lotus/system/platforms/windows/window.h"

/// \file
/// Implementation of windows on Windows.

#include <cstdint>

#include "lotus/system/platforms/windows/details.h"

namespace lotus::system::platforms::windows {
	window::~window() {
		if (_hwnd) {
			_details::assert_win32(DestroyWindow(_hwnd));
		}
	}

	window::window(window &&src) : _hwnd(std::exchange(src._hwnd, nullptr)) {
		_update_address();
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

	void window::acquire_mouse_capture() {
		SetCapture(_hwnd);
	}

	void window::release_mouse_capture() {
		_details::assert_win32(ReleaseCapture());
	}

	cvec2s window::get_size() const {
		RECT rect = {};
		_details::assert_win32(GetClientRect(_hwnd, &rect));
		return cvec2s(static_cast<std::size_t>(rect.right), static_cast<std::size_t>(rect.bottom));
	}

	void window::set_title(std::u8string_view title) {
		auto bookmark = memory::stack_allocator::for_this_thread().bookmark();
		auto alloc = bookmark.create_std_allocator<TCHAR>();
		auto title_tstring = _details::u8string_to_tstring(title, alloc);
		_details::assert_win32(SetWindowText(_hwnd, title_tstring.c_str()));
	}

	window::window(HWND hwnd) : _hwnd(hwnd) {
		_update_address();
	}

	void window::_update_address() {
		if (_hwnd) {
			SetLastError(0);
			SetWindowLongPtr(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
			_details::assert_win32(GetLastError() == 0);
		}
	}
}
