#include "lotus/system/platforms/windows/application.h"

/// \file
/// Implementation of Windows applications.

#include "lotus/system/platforms/windows/details.h"
#include "lotus/utils/stack_allocator.h"

namespace lotus::system::platforms::windows {
	LRESULT CALLBACK _window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}


	application::application(std::u8string_view name) {
		auto bookmark = stack_allocator::scoped_bookmark::create();

		auto class_name = _details::u8string_to_tstring(name, stack_allocator::allocator<TCHAR>::for_this_thread());

		WNDCLASSEX wcex;
		std::memset(&wcex, 0, sizeof(wcex));
		wcex.cbSize = sizeof(wcex);
		wcex.hInstance = GetModuleHandle(nullptr);
		_details::assert_win32(wcex.hCursor = LoadCursor(nullptr, IDC_ARROW));
		wcex.lpfnWndProc = _window_proc;
		wcex.lpszClassName = class_name.c_str();
		_details::assert_win32(_window_class = RegisterClassEx(&wcex));
	}

	application::~application() {
		_details::assert_win32(UnregisterClass(
			reinterpret_cast<LPCTSTR>(static_cast<std::uintptr_t>(_window_class)), GetModuleHandle(nullptr)
		));
	}

	window application::create_window() const {
		HWND wnd = CreateWindowEx(
			WS_EX_APPWINDOW, reinterpret_cast<LPCTSTR>(static_cast<std::uintptr_t>(_window_class)),
			TEXT("window"), WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			nullptr, nullptr, GetModuleHandle(nullptr), nullptr
		);
		_details::assert_win32(wnd);
		return window(wnd);
	}

	message_type application::process_message_blocking() {
		MSG msg;
		_details::assert_win32(GetMessage(&msg, nullptr, 0, 0) != -1);
		CallWindowProc(_window_proc, msg.hwnd, msg.message, msg.wParam, msg.lParam);
		return msg.message == WM_QUIT ? message_type::quit : message_type::normal;
	}

	message_type application::process_message_nonblocking() {
		MSG msg;
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			CallWindowProc(_window_proc, msg.hwnd, msg.message, msg.wParam, msg.lParam);
			return msg.message == WM_QUIT ? message_type::quit : message_type::normal;
		}
		return message_type::none;
	}
}
