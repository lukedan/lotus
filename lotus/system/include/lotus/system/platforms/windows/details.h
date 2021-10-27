#pragma once

/// \file
/// Utility functions for Windows.

#include <string>
#include <iostream>

#include <Windows.h>

namespace lotus::system::platforms::windows::_details {
	/// Aborts if the given value is zero.
	template <typename V> inline void assert_win32(V value) {
		if (value == static_cast<V>(0)) {
			// TOOD output the error
			std::abort();
		}
	}
	/// Aborts if the given \p HRESULT does not indicate success.
	inline void assert_com(HRESULT hr) {
		if (FAILED(hr)) {
			std::cerr << "COM error: " << hr << std::endl;
			std::abort();
		}
	}

	/// Converts the given UTF-8 string to strings usable for calling Windows API.
	template <typename Allocator> [[nodiscard]] inline std::basic_string<
		TCHAR, std::char_traits<TCHAR>, Allocator
	> u8string_to_tstring(std::u8string_view view, const Allocator &allocator = Allocator()) {
		using _string_t = std::basic_string<TCHAR, std::char_traits<TCHAR>, Allocator>;

#ifdef UNICODE
		if (view.empty()) {
			return _string_t(allocator);
		}
		int count = MultiByteToWideChar(
			CP_UTF8, 0, reinterpret_cast<LPCCH>(view.data()), static_cast<int>(view.size()), nullptr, 0
		);
		assert_win32(count);
		_string_t result(count - 1, 0, allocator);
		assert_win32(MultiByteToWideChar(
			CP_UTF8, 0, reinterpret_cast<LPCCH>(view.data()), static_cast<int>(view.size()), result.data(), count
		));
		return result;
#else
		return _string_t(reinterpret_cast<const TCHAR*>(view.data()), view.size(), allocator);
#endif
	}
	/// Converts the given wide Windows string to a UTF-8 string.
	template <typename Allocator> [[nodiscard]] inline std::basic_string<
		char8_t, std::char_traits<char8_t>, Allocator
	> wstring_to_u8string(std::basic_string_view<WCHAR> view, const Allocator &allocator = Allocator()) {
		// TODO test surrogate chars
		int len = WideCharToMultiByte(
			CP_UTF8, 0, view.data(), static_cast<int>(view.size()), nullptr, 0, nullptr, nullptr
		);
		assert_win32(len != 0);
		std::u8string res(static_cast<std::size_t>(len), static_cast<char8_t>(0), allocator);
		assert_win32(WideCharToMultiByte(
			CP_UTF8, 0, view.data(), static_cast<int>(view.size()),
			reinterpret_cast<LPSTR>(res.data()), len, nullptr, nullptr
		) == len);
		// remove duplicate null terminator that WideCharToMultiByte() introduces when cchWideChar is -1
		res.pop_back();
		return res;
	}
	/// \overload
	template <typename Allocator> [[nodiscard]] inline std::basic_string<
		char8_t, std::char_traits<char8_t>, Allocator
	> wstring_to_u8string(const WCHAR *str, const Allocator &allocator = Allocator()) {
		int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
		assert_win32(len != 0);
		std::u8string res(static_cast<std::size_t>(len), static_cast<char8_t>(0), allocator);
		assert_win32(WideCharToMultiByte(
			CP_UTF8, 0, str, -1, reinterpret_cast<LPSTR>(res.data()), len, nullptr, nullptr
		) == len);
		// remove duplicate null terminator that WideCharToMultiByte() introduces when cchWideChar is -1
		res.pop_back();
		return res;
	}
	/// Converts the given Windows string to a UTF-8 string.
	template <typename Allocator> [[nodiscard]] inline std::basic_string<
		char8_t, std::char_traits<char8_t>, Allocator
	> tstring_to_u8string(std::basic_string_view<TCHAR> view, const Allocator &allocator = Allocator()) {
#ifdef UNICODE
		return wstring_to_u8string(view, allocator);
#else
		return std::basic_string<char8_t, std::char_traits<char8_t>, Allocator>(
			reinterpret_cast<const char8_t*>(view.data()), view.size(), allocator
		);
#endif
	}
	/// \overload
	template <typename Allocator> [[nodiscard]] inline std::basic_string<
		char8_t, std::char_traits<char8_t>, Allocator
	> tstring_to_u8string(const TCHAR *str, const Allocator &allocator = Allocator()) {
#ifdef UNICODE
		return wstring_to_u8string(str, allocator);
#else
		return std::basic_string<char8_t, std::char_traits<char8_t>, Allocator>(
			reinterpret_cast<const char8_t*>(view.data()), view.size(), allocator
		);
#endif
	}
}
