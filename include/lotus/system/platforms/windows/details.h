#pragma once

/// \file
/// Utility functions for Windows.

#include <string>
#include <iostream>

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
}
