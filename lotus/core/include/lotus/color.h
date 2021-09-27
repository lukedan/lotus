#pragma once

/// \file
/// Colors.

#include "common.h"

namespace lotus {
	/// Linear RGBA colors.
	template <typename T> struct linear_rgba {
	public:
		/// No initialization.
		linear_rgba(uninitialized_t) {
		}
		/// Initializes all channels to zero.
		constexpr linear_rgba(zero_t) : r(zero), g(zero), b(zero), a(zero) {
		}
		/// Initializes all channels.
		constexpr linear_rgba(T rr, T gg, T bb, T aa) :
			r(std::move(rr)), g(std::move(gg)), b(std::move(bb)), a(std::move(aa)) {
		}
		/// Creates an fully opaque color.
		[[nodiscard]] constexpr static inline linear_rgba create_opaque(T r, T g, T b) {
			return linear_rgba(std::move(r), std::move(g), std::move(b), static_cast<T>(1));
		}
		/// Creates an fully transparent color.
		[[nodiscard]] constexpr static inline linear_rgba create_transparent(T r, T g, T b) {
			return linear_rgba(std::move(r), std::move(g), std::move(b), zero);
		}

		T r; ///< The red channel.
		T g; ///< The green channel.
		T b; ///< The blue channel.
		T a; ///< The alpha channel.
	};

	using linear_rgba_f = linear_rgba<float>; ///< Linear RGBA colors of \p float.
	using linear_rgba_d = linear_rgba<double>; ///< Linear RGBA colors of \p double.
}
