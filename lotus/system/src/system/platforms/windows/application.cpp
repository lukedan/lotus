#include "lotus/system/platforms/windows/application.h"

/// \file
/// Implementation of Windows applications.

#include <windowsx.h>

#include "lotus/system/platforms/windows/details.h"
#include "lotus/system/window.h"
#include "lotus/memory/stack_allocator.h"

namespace lotus::system::platforms::windows {
	[[nodiscard]] static modifier_key_mask _get_modifier_key_mask(WPARAM wparam) {
		return
			((wparam & MK_CONTROL)    ? modifier_key_mask::control : modifier_key_mask::none) |
			((wparam & MK_SHIFT)      ? modifier_key_mask::shift   : modifier_key_mask::none) |
			(GetKeyState(VK_MENU) < 0 ? modifier_key_mask::alt     : modifier_key_mask::none);
	}
	LRESULT CALLBACK _window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto *wnd = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		auto *sys_wnd = static_cast<system::window*>(wnd);
		switch (msg) {
		case WM_SIZE:
			{
				window_events::resize info(cvec2s(LOWORD(lparam), HIWORD(lparam)));
				if (sys_wnd->on_resize) {
					sys_wnd->on_resize(info);
				}
			}
			return 0;

		case WM_CANCELMODE:
			{
				_details::assert_win32(ReleaseCapture());
				if (sys_wnd->on_capture_broken) {
					sys_wnd->on_capture_broken();
				}
			}
			return 0;

		case WM_CLOSE:
			{
				window_events::close_request info;
				if (sys_wnd->on_close_request) {
					sys_wnd->on_close_request(info);
				}
				if (info.should_close) {
					_details::assert_win32(DestroyWindow(wnd->_hwnd));
					wnd->_hwnd = nullptr;
				}
			}
			return 0;

		case WM_MOUSEMOVE:
			{
				window_events::mouse::move info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)), _get_modifier_key_mask(wparam)
				);
				if (sys_wnd->on_mouse_move) {
					sys_wnd->on_mouse_move(info);
				}
			}
			return 0;

		case WM_LBUTTONDOWN:
			{
				window_events::mouse::button_down info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
					mouse_button::primary, _get_modifier_key_mask(wparam)
				);
				if (sys_wnd->on_mouse_button_down) {
					sys_wnd->on_mouse_button_down(info);
				}
			}
			return 0;
		case WM_LBUTTONUP:
			{
				window_events::mouse::button_up info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
					mouse_button::primary, _get_modifier_key_mask(wparam)
				);
				if (sys_wnd->on_mouse_button_up) {
					sys_wnd->on_mouse_button_up(info);
				}
			}
			return 0;

		case WM_RBUTTONDOWN:
			{
				window_events::mouse::button_down info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
					mouse_button::secondary, _get_modifier_key_mask(wparam)
				);
				if (sys_wnd->on_mouse_button_down) {
					sys_wnd->on_mouse_button_down(info);
				}
			}
			return 0;
		case WM_RBUTTONUP:
			{
				window_events::mouse::button_up info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
					mouse_button::secondary, _get_modifier_key_mask(wparam)
				);
				if (sys_wnd->on_mouse_button_up) {
					sys_wnd->on_mouse_button_up(info);
				}
			}
			return 0;
		case WM_MBUTTONDOWN:
			{
				window_events::mouse::button_down info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
					mouse_button::middle, _get_modifier_key_mask(wparam)
				);
				if (sys_wnd->on_mouse_button_down) {
					sys_wnd->on_mouse_button_down(info);
				}
			}
			return 0;
		case WM_MBUTTONUP:
			{
				window_events::mouse::button_up info(
					cvec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)),
					mouse_button::middle, _get_modifier_key_mask(wparam)
				);
				if (sys_wnd->on_mouse_button_up) {
					sys_wnd->on_mouse_button_up(info);
				}
			}
			return 0;

		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		assert(false); // event not properly handled
	}


	application::application(std::u8string_view name) {
		auto bookmark = get_scratch_bookmark();

		auto class_name = _details::u8string_to_tstring(name, bookmark.create_std_allocator<TCHAR>());

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
