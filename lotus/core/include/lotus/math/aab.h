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
		/// Zero initialize by default.
		constexpr aab() : aab(zero) {
		}
		/// Initializes the box to a singularity at the origin.
		constexpr aab(zero_t) : min(zero), max(zero) {
		}
		/// Creates an axis-aligned box with its corners initializezd to the given values.
		[[nodiscard]] constexpr static aab create_from_min_max(vector_type minp, vector_type maxp) {
			return aab(std::move(minp), std::move(maxp));
		}
		/// Creates an AAB based on its center and half its size.
		[[nodiscard]] constexpr static aab create_from_center_half_size(vector_type center, vector_type half_size) {
			return aab(center - half_size, center + half_size);
		}
		/// Creates an axis-aligned box with zero volume at the given position.
		[[nodiscard]] constexpr static aab create_singularity(vector_type v) {
			return aab(v, v);
		}
		/// Creates an AAB that contains all possible values. If T is floating point, the AAB will be [-inf, inf].
		/// If T is integral, the AAB will be [min, max].
		[[nodiscard]] constexpr static aab create_infinity() {
			if constexpr (std::is_floating_point_v<T>) {
				return aab(
					-vector_type::filled(std::numeric_limits<T>::infinity()),
					vector_type::filled(std::numeric_limits<T>::infinity())
				);
			} else {
				return aab(
					vector_type::filled(std::numeric_limits<T>::min()),
					vector_type::filled(std::numeric_limits<T>::max())
				);
			}
		}

		/// Returns the minimum AAB that contains all the given AABs.
		[[nodiscard]] constexpr static aab minimum_containing(std::span<const aab> aabs) {
			if (aabs.empty()) {
				return zero;
			}
			aab result = aabs[0];
			for (usize i = 1; i < aabs.size(); ++i) {
				result.min = matm::min(result.min, aabs[i].min);
				result.max = matm::max(result.max, aabs[i].max);
			}
			return result;
		}
		/// \overload
		[[nodiscard]] constexpr static aab minimum_containing(std::initializer_list<aab> aabs) {
			return minimum_containing({ aabs.begin(), aabs.end() });
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

		/// Returns whether the two boxes intersect. Boxes are treated as if they don't contain the surfaces.
		[[nodiscard]] constexpr static bool intersects(const aab &lhs, const aab &rhs) {
			return matm::less(lhs.min, rhs.max).all_true() && matm::greater(lhs.max, rhs.min).all_true();
		}
		/// Returns whether the other AAB is fully contained by this AAB.
		[[nodiscard]] constexpr bool contains(const aab &other) const {
			return
				matm::greater_or_equal(other.min, min).all_true() &&
				matm::less_or_equal(other.max, max).all_true();
		}

		/// Returns an AAB with \ref min and \ref max swapped.
		[[nodiscard]] constexpr aab negated() const {
			return aab(max, min);
		}

		/// Default equality and inequality comparisons.
		[[nodiscard]] friend bool operator==(const aab&, const aab&) = default;

		vector_type min = uninitialized; ///< Point with the smallest coordinates contained by this box.
		vector_type max = uninitialized; ///< Point with the largest coordinates contained by this box.
	protected:
		/// Initializes all fields of this struct.
		constexpr aab(vector_type minp, vector_type maxp) : min(std::move(minp)), max(std::move(maxp)) {
		}
	};


	template <typename T> using aab2 = aab<2, T>; ///< Two-dimensional axis-aligned boxes.
	using aab2f32 = aab2<f32>;   ///< Two-dimensional axis-aligned boxes of \ref f32.
	using aab2f64 = aab2<f64>;   ///< Two-dimensional axis-aligned boxes of \ref f64.
	using aab2u32 = aab2<u32>;   ///< Two-dimensional axis-aligned boxes of \ref u32.

	template <typename T> using aab3 = aab<3, T>; ///< Three-dimensional axis-aligned boxes.
	using aab3f32 = aab3<f32>;   ///< Three-dimensional axis-aligned boxes of \ref f32.
	using aab3f64 = aab3<f64>;   ///< Three-dimensional axis-aligned boxes of \ref f64.
}
