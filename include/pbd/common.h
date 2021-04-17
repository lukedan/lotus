#pragma once

/// \file
/// Common types and functions.

#include <type_traits>

namespace pbd {
	/// A type indicating a specific object should not be initialized.
	struct uninitialized_t {
		/// Implicit conversion to arithmetic types and pointers.
		template <
			typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_pointer_v<T>, int> = 0
		> operator T() const {
			return T{};
		}
	};
	/// A type indicating a specific object should be zero-initialized.
	struct zero_t {
		/// Implicit conversion to arithmetic types and pointers.
		template <
			typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_pointer_v<T>, int> = 0
		> constexpr operator T() const {
			return static_cast<T>(0);
		}
	};
	constexpr inline uninitialized_t uninitialized; ///< An instance of \ref uninitialized_t.
	constexpr inline zero_t zero; ///< An instance of \ref zero_t.
}
