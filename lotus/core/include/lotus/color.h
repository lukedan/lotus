#pragma once

/// \file
/// Colors.

#include <algorithm>

#include "common.h"

namespace lotus {
	/// Linear RGBA colors.
	template <typename T> struct linear_rgba {
		static_assert(
			!(std::is_integral_v<T> && std::is_signed_v<T>),
			"Please use unsigned integral types for color"
		);
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

		/// Converts the color into another format. For floating-point types, the output will be in [0, 1]; for
		/// integral types, they will occupy the full range.
		template <typename U> constexpr linear_rgba<U> into() const {
			if constexpr (std::is_integral_v<T>) {
				if constexpr (std::is_integral_v<U>) {
					return into<float>().into<U>(); // TODO better approach?
				} else {
					return linear_rgba<U>(
						static_cast<U>(r) / std::numeric_limits<T>::max(),
						static_cast<U>(g) / std::numeric_limits<T>::max(),
						static_cast<U>(b) / std::numeric_limits<T>::max(),
						static_cast<U>(a) / std::numeric_limits<T>::max()
					);
				}
			} else {
				if constexpr (std::is_integral_v<U>) {
					return linear_rgba<U>(
						static_cast<U>(std::clamp<T>(r, 0.0f, 1.0f) * std::numeric_limits<T>::max() + 0.5f),
						static_cast<U>(std::clamp<T>(g, 0.0f, 1.0f) * std::numeric_limits<T>::max() + 0.5f),
						static_cast<U>(std::clamp<T>(b, 0.0f, 1.0f) * std::numeric_limits<T>::max() + 0.5f),
						static_cast<U>(std::clamp<T>(a, 0.0f, 1.0f) * std::numeric_limits<T>::max() + 0.5f)
					);
				} else {
					return linear_rgba<U>(
						static_cast<U>(r), static_cast<U>(g), static_cast<U>(b), static_cast<U>(a)
					);
				}
			}
		}

		T r; ///< The red channel.
		T g; ///< The green channel.
		T b; ///< The blue channel.
		T a; ///< The alpha channel.
	};

	using linear_rgba_f = linear_rgba<float>; ///< Linear RGBA colors of \p float.
	using linear_rgba_d = linear_rgba<double>; ///< Linear RGBA colors of \p double.
	using linear_rgba_u8 = linear_rgba<std::uint8_t>; ///< Linear RGBA colors of \p std::uint8_t.
}
