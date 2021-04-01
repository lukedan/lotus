#pragma once

/// \file
/// Vector operations.

#include "matrix.h"

namespace pbd {
	/// Column vector.
	template <std::size_t Dim, typename T> using column_vector = matrix<Dim, 1, T>;
	/// Row vector.
	template <std::size_t Dim, typename T> using row_vector = matrix<1, Dim, T>;


	/// Generic vector utilities.
	class vec {
	public:
		/// Creates a vector with the given elements.
		template <typename Vec> [[nodiscard]] inline static constexpr Vec create(
			std::initializer_list<typename Vec::value_type> val
		) {
			assert(val.size() == Vec::dimensionality);
			Vec result = zero;
			for (auto it = val.begin(); it != val.end(); ++it) {
				result[it - val.begin()] = std::move(*it);
			}
			return result;
		}

		/// Dot product.
		template <typename Vec> [[nodiscard]] inline static constexpr typename Vec::value_type dot(
			const Vec &lhs, const Vec &rhs
		) {
			auto result = static_cast<typename Vec::value_type>(0);
			for (std::size_t i = 0; i < Vec::dimensionality; ++i) {
				result += lhs[i] * rhs[i];
			}
			return result;
		}
		template <
			typename Vec
		> [[nodiscard]] inline static constexpr std::enable_if_t<Vec::dimensionality == 3, Vec> cross(
			const Vec &lhs, const Vec &rhs
		) {
			return create<Vec>({
				lhs[1] * rhs[2] - lhs[2] * rhs[1],
				lhs[2] * rhs[0] - lhs[0] * rhs[2],
				lhs[0] * rhs[1] - lhs[1] * rhs[0]
			});
		}

		/// Normalizes the given vector without any safety checks.
		template <typename Vec> [[nodiscard]] inline static constexpr Vec unsafe_normalize(Vec v) {
			v /= v.norm();
			return std::move(v);
		}
	};

	/// Column vector utilities.
	template <std::size_t Dim, typename T> class cvec {
	public:
		/// The corresponding matrix type that's used to store the matrix.
		using matrix_type = column_vector<Dim, T>;

		/// This class only contains utility functions.
		cvec() = delete;

		/// Shorthand for \ref vec::create().
		[[nodiscard]] inline static constexpr matrix_type create(std::initializer_list<T> val) {
			return vec::create<matrix_type>(val);
		}
	};


	template <typename T> using cvec2_t = column_vector<2, T>; ///< 2D column vectors.
	using cvec2f_t = cvec2_t<float>; ///< 2D column vectors of \p float.
	using cvec2d_t = cvec2_t<double>; ///< 2D column vectors of \p double.

	template <typename T> using cvec3_t = column_vector<3, T>; ///< 3D column vectors.
	using cvec3f_t = cvec3_t<float>; ///< 3D column vectors of \p float.
	using cvec3d_t = cvec3_t<double>; ///< 3D column vectors of \p double.

	template <typename T> using cvec4_t = column_vector<4, T>; ///< 4D column vectors.
	using cvec4f_t = cvec4_t<float>; ///< 4D column vectors of \p float.
	using cvec4d_t = cvec4_t<double>; ///< 4D column vectors of \p double.


	template <typename T> using rvec2_t = row_vector<2, T>; ///< 2D row vectors.
	using rvec2f_t = rvec2_t<float>; ///< 2D row vectors of \p float.
	using rvec2d_t = rvec2_t<double>; ///< 2D row vectors of \p double.

	template <typename T> using rvec3_t = row_vector<3, T>; ///< 3D row vectors.
	using rvec3f_t = rvec3_t<float>; ///< 3D row vectors of \p float.
	using rvec3d_t = rvec3_t<double>; ///< 3D row vectors of \p double.

	template <typename T> using rvec4_t = row_vector<4, T>; ///< 4D row vectors.
	using rvec4f_t = rvec4_t<float>; ///< 4D row vectors of \p float.
	using rvec4d_t = rvec4_t<double>; ///< 4D row vectors of \p double.


	template <typename T> using cvec2 = cvec<2, T>; ///< Utility for 2D column vectors.
	using cvec2f = cvec2<float>; ///< Utility for 2D column vectors of \p float.
	using cvec2d = cvec2<double>; ///< Utility for 2D column vectors of \p double.

	template <typename T> using cvec3 = cvec<3, T>; ///< Utility for 3D column vectors.
	using cvec3f = cvec3<float>; ///< Utility for 3D column vectors of \p float.
	using cvec3d = cvec3<double>; ///< Utility for 3D column vectors of \p double.

	template <typename T> using cvec4 = cvec<4, T>; ///< Utility for 4D column vectors.
	using cvec4f = cvec4<float>; ///< Utility for 4D column vectors of \p float.
	using cvec4d = cvec4<double>; ///< Utility for 4D column vectors of \p double.
}
