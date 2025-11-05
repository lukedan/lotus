#pragma once

/// \file
/// Numeric traits.

#include <cmath>

namespace lotus {
	/// Traits of a numeric type.
	template <typename T> struct numeric_traits {
		using value_type = T; ///< Value type.

		/// Square root.
		[[nodiscard]] constexpr static value_type sqrt(T v) {
			return std::sqrt(v);
		}
	};
}
