#include "lotus/system/platforms/windows/application.h"

/// \file
/// Implementation of Windows applications.

#include <windowsx.h>

#include "lotus/system/platforms/windows/details.h"
#include "lotus/system/window.h"
#include "lotus/utils/stack_allocator.h"

namespace lotus::system::platforms::windows {
	LRESULT CALLBACK _window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto *wnd = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		auto *sys_wnd = static_cast<system::window*>(wnd);
		switch (msg) {
		case WM_SIZE:
			{
				window_events::resize info(cvec2s(LOWORD(lparam), HIWORD(lparam)));
				sys_wnd->on_resize.invoke_all(*sys_wnd, info);
			}
			return 0;

		case WM_CANCELMODE:
			sys_wnd->on_capture_broken.invoke_all(*sys_wnd);
			return 0;

		case WM_CLOSE:
			{
				window_events::close_request info;
				sys_wnd->on_close_request.invoke_all(*sys_wnd, info);
				if (info.should_close) {
					_details::assert_win32(DestroyWindow(wnd->_hwnd));
					wnd->_hwnd = nullptr;
				}
			}
			return 0;

		case WM_MOUSEMOVE:
			{
				window_events::mouse::move info(cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
				sys_wnd->on_mouse_move.invoke_all(*sys_wnd, info);
			}
			return 0;

		case WM_LBUTTONDOWN:
			{
				window_events::mouse::button_down info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)), mouse_button::primary
				);
				sys_wnd->on_mouse_button_down.invoke_all(*sys_wnd, info);
			}
			return 0;
		case WM_LBUTTONUP:
			{
				window_events::mouse::button_up info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)), mouse_button::primary
				);
				sys_wnd->on_mouse_button_up.invoke_all(*sys_wnd, info);
			}
			return 0;

		case WM_RBUTTONDOWN:
			{
				window_events::mouse::button_down info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)), mouse_button::secondary
				);
				sys_wnd->on_mouse_button_down.invoke_all(*sys_wnd, info);
			}
			return 0;
		case WM_RBUTTONUP:
			{
				window_events::mouse::button_up info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)), mouse_button::secondary
				);
				sys_wnd->on_mouse_button_up.invoke_all(*sys_wnd, info);
			}
			return 0;
		case WM_MBUTTONDOWN:
			{
				window_events::mouse::button_down info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)), mouse_button::middle
				);
				sys_wnd->on_mouse_button_down.invoke_all(*sys_wnd, info);
			}
			return 0;
		case WM_MBUTTONUP:
			{
				window_events::mouse::button_up info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)), mouse_button::middle
				);
				sys_wnd->on_mouse_button_up.invoke_all(*sys_wnd, info);
			}
			return 0;

		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		assert(false); // event not properly handled
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

	void application::quit() {
		PostQuitMessage(0);
	}
}
