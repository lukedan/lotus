#pragma once

/// \file
/// An index type where a specific value is used to indicate that the index is invalid.

#include <cstddef>
#include <limits>
#include <utility>

#include "lotus/common.h"

namespace lotus::index {
	/// Struct used to mark if an enum type is an index type.
	template <typename T> struct is_index_type : std::false_type {
	};
	/// Shorthand for \ref is_index_type::value.
	template <typename T> constexpr bool is_index_type_v = is_index_type<T>::value;

	/// Returns the value of the index incremented by 1.
	template <typename T, std::enable_if_t<is_index_type_v<T>, int> = 0> [[nodiscard]] constexpr T next(T value) {
		return static_cast<T>(std::to_underlying(value) + 1);
	}
	/// Returns the value of the index decremented by 1.
	template <typename T, std::enable_if_t<is_index_type_v<T>, int> = 0> [[nodiscard]] constexpr T prev(T value) {
		return static_cast<T>(std::to_underlying(value) - 1);
	}
}
