#pragma once

/// \file
/// Axis-aligned boxes.

#include <cstddef>

#include "vector.h"

namespace lotus {
	/// An axis-aligned box.
	template <
		usize Dim, typename T, template <usize, typename> typename Vec = column_vector
	> struct aab {
	public:
		using vector_type = Vec<Dim, T>; ///< Vector type.
		using value_type = T; ///< Scalar value type.
		constexpr static usize dimensionality = Dim; ///< The dimensionality of this box.

		/// No initialization.
		aab(uninitialized_t) {
		}
		/// Initializes the box to a singularity at the origin.
		constexpr aab(zero_t) : min(zero), max(zero) {
		}
		/// Creates an axis-aligned box with its corners initializezd to the given values.
		[[nodiscard]] constexpr inline static aab create_from_min_max(vector_type minp, vector_type maxp) {
			return aab(std::move(minp), std::move(maxp));
		}
		/// Creates an axis-aligned box with zero volume at the given position.
		[[nodiscard]] constexpr inline static aab create_singularity(vector_type v) {
			return aab(v, v);
		}

		/// Returns the signed size of this box.
		[[nodiscard]] constexpr vector_type signed_size() const {
			return max - min;
		}
		/// Returns the signed volume of this box.
		[[nodiscard]] constexpr value_type signed_volume() const {
			vector_type size = signed_size();
			value_type result = size[0];
			for (usize i = 1; i < Dim; ++i) {
				result *= size[i];
			}
			return result;
		}

		vector_type min = uninitialized; ///< Point with the smallest coordinates contained by this box.
		vector_type max = uninitialized; ///< Point with the largest coordinates contained by this box.
	protected:
		/// Initializes all fields of this struct.
		constexpr aab(vector_type minp, vector_type maxp) : min(std::move(minp)), max(std::move(maxp)) {
		}
	};


	template <typename T> using aab2 = aab<2, T>; ///< Two-dimensional axis-aligned boxes.
	using aab2f   = aab2<float>;  ///< Two-dimensional axis-aligned boxes of \p float.
	using aab2d   = aab2<double>; ///< Two-dimensional axis-aligned boxes of \p double.
	using aab2i   = aab2<int>;    ///< Two-dimensional axis-aligned boxes of \p int.
	using aab2s   = aab2<usize>;  ///< Two-dimensional axis-aligned boxes of \ref usize.
	using aab2u32 = aab2<u32>;    ///< Two-dimensional axis-aligned boxes of \ref u32.

	template <typename T> using aab3 = aab<3, T>; ///< Three-dimensional axis-aligned boxes.
	using aab3f = aab3<float>;  ///< Three-dimensional axis-aligned boxes of \p float.
	using aab3d = aab3<double>; ///< Three-dimensional axis-aligned boxes of \p double.
	using aab3i = aab3<int>;    ///< Three-dimensional axis-aligned boxes of \p int.
	using aab3s = aab3<usize>;  ///< Three-dimensional axis-aligned boxes of \ref usize.
}
