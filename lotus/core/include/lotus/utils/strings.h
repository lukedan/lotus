#pragma once

/// \file
/// String utilities.

#include <array>
#include <string_view>

#include "lotus/types.h"

/// String utilities.
namespace lotus::string {
	/// Assumes that the given \p string_view contains UTF-8 text and converts it into a \p std::u8string_view.
	[[nodiscard]] inline std::u8string_view assume_utf8(std::string_view str) {
		return std::u8string_view(reinterpret_cast<const char8_t*>(str.data()), str.size());
	}
	/// Converts a UTF-8 string view to a generic string.
	[[nodiscard]] inline std::string_view to_generic(std::u8string_view str) {
		return std::string_view(reinterpret_cast<const char*>(str.data()), str.size());
	}

	/// Splits the string with the given separator and calls the callback function with each substring.
	template <typename Char, typename Callback> void split(
		std::basic_string_view<Char> full, std::basic_string_view<Char> patt, Callback &&callback
	) {
		auto beg = full.begin();
		for (; beg != full.end(); ) {
			for (auto end = beg; ; ++end) {
				if (end == full.end()) {
					callback(std::basic_string_view(beg, full.end()));
					return;
				}
				if (
					static_cast<usize>(full.end() - end) >= patt.size() &&
					std::basic_string_view<Char>(end, end + patt.size()) == patt
				) {
					callback(std::basic_string_view<Char>(beg, end));
					beg = end + patt.size();
					break;
				}
			}
		}
	}

	/// A compile-time string.
	template <typename Char, usize N> struct constexpr_string {
		std::array<Char, N> contents; ///< The contents of this string, including the final terminating zero.

		/// Default-initializes \ref contents.
		consteval constexpr_string(std::nullptr_t) : contents{} {
		}
		/// Initializes the string.
		consteval constexpr_string(const Char (&str)[N]) {
			std::copy(std::begin(str), std::end(str), contents.begin());
		}

		/// Implicit conversion to a string view.
		[[nodiscard]] consteval operator std::basic_string_view<Char>() const {
			return std::basic_string_view<Char>(contents.data(), contents.size() - 1);
		}

		/// Converts this string element-wise into another type.
		template <typename ToChar> [[nodiscard]] inline consteval constexpr_string<ToChar, N> as() const {
			constexpr_string<ToChar, N> result(nullptr);
			for (usize i = 0; i < N; ++i) {
				result.contents[i] = static_cast<ToChar>(contents[i]);
			}
			return result;
		}
	};
}
