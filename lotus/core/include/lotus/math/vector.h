#pragma once

/// \file
/// Vector operations.

#include "lotus/math/matrix.h"

namespace lotus {
	/// Column vector.
	template <usize Dim, typename T> using column_vector = matrix<Dim, 1, T>;
	/// Row vector.
	template <usize Dim, typename T> using row_vector = matrix<1, Dim, T>;


	/// Generic vector utilities.
	class vec : public mat {
	public:
		/// Dot product.
		template <typename VecLhs, typename VecRhs> [[nodiscard]] static constexpr auto dot(
			const VecLhs &lhs, const VecRhs &rhs
		) {
			static_assert(VecLhs::dimensionality == VecRhs::dimensionality, "Dimensionality mismatch");
			auto result = lhs[0] * rhs[0];
			for (usize i = 1; i < VecLhs::dimensionality; ++i) {
				result += lhs[i] * rhs[i];
			}
			return result;
		}
		/// Cross product.
		template <typename VecLhs, typename VecRhs> [[nodiscard]] constexpr static auto cross(
			const VecLhs &lhs, const VecRhs &rhs
		) {
			static_assert(VecLhs::dimensionality == 3 && VecRhs::dimensionality == 3, "Incorrect dimensionality");
			return matrix<VecLhs::num_rows, VecLhs::num_columns, decltype(lhs[0] * rhs[0])>{
				lhs[1] * rhs[2] - lhs[2] * rhs[1],
				lhs[2] * rhs[0] - lhs[0] * rhs[2],
				lhs[0] * rhs[1] - lhs[1] * rhs[0]
			};
		}
		/// Returns the matrix that can be used to compute the cross product between two vectors, according to
		/// Wikipedia's definitions: https://en.wikipedia.org/wiki/Cross_product#Conversion_to_matrix_multiplication
		/// This function returns F(v) for v such that <tt>a x b = F(a) b = F(b)^T a</tt>.
		template <typename Vec> [[nodiscard]] constexpr static std::enable_if_t<
			Vec::dimensionality == 3, matrix<3, 3, typename Vec::value_type>
		> cross_matrix(const Vec &v) {
			return {
				{ 0, -v[2], v[1] },
				{ v[2], 0, -v[0] },
				{ -v[1], v[0], 0 }
			};
		}
	};

	/// Unsafe vector operations.
	class vec_unsafe {
	public:
		/// Prevents instances of this type from being created.
		vec_unsafe() = delete;

		/// Normalizes the given vector without any safety checks.
		template <typename Vec> [[nodiscard]] static constexpr Vec normalize(Vec v) {
			v /= v.norm();
			return v;
		}
	};
	using vecu = vec_unsafe; ///< Shorthand for \ref vec_unsafe.


	/// Shorthands for various vector types.
	inline namespace vector_types {
		template <typename T> using cvec2 = column_vector<2, T>; ///< 2D column vectors.
		using cvec2f32 = cvec2<f32>;   ///< 2D column vectors of \ref f32.
		using cvec2f64 = cvec2<f64>;   ///< 2D column vectors of \ref f64.
		using cvec2u8  = cvec2<u8>;    ///< 2D column vectors of \ref u8.
		using cvec2i   = cvec2<int>;   ///< 2D column vectors of \p int.
		using cvec2s   = cvec2<usize>; ///< 2D column vectors of \ref usize.
		using cvec2u32 = cvec2<u32>;   ///< 2D column vectors of \ref u32.

		template <typename T> using cvec3 = column_vector<3, T>; ///< 3D column vectors.
		using cvec3f32 = cvec3<f32>;   ///< 3D column vectors of \ref f32.
		using cvec3f64 = cvec3<f64>;   ///< 3D column vectors of \ref f64.
		using cvec3i   = cvec3<int>;   ///< 3D column vectors of \p int.
		using cvec3s   = cvec3<usize>; ///< 3D column vectors of \ref usize.
		using cvec3u32 = cvec3<u32>;   ///< 2D column vectors of \ref u32.

		template <typename T> using cvec4 = column_vector<4, T>; ///< 4D column vectors.
		using cvec4f32 = cvec4<f32>;   ///< 4D column vectors of \ref f32.
		using cvec4f64 = cvec4<f64>;   ///< 4D column vectors of \ref f64.
		using cvec4i   = cvec4<int>;   ///< 4D column vectors of \p int.
		using cvec4s   = cvec4<usize>; ///< 4D column vectors of \ref usize.
		using cvec4u32 = cvec4<u32>;   ///< 4D column vectors of \ref u32.

		template <typename T> using rvec2 = row_vector<2, T>; ///< 2D row vectors.
		using rvec2f32 = rvec2<f32>; ///< 2D row vectors of \ref f32.
		using rvec2f64 = rvec2<f64>; ///< 2D row vectors of \ref f64.
		using rvec2i   = rvec2<int>; ///< 2D row vectors of \p int.
		using rvec2u32 = rvec2<u32>; ///< 2D row vectors of \ref u32.

		template <typename T> using rvec3 = row_vector<3, T>; ///< 3D row vectors.
		using rvec3f32 = rvec3<f32>; ///< 3D row vectors of \ref f32.
		using rvec3f64 = rvec3<f64>; ///< 3D row vectors of \ref f64.
		using rvec3i   = rvec3<int>; ///< 3D row vectors of \p int.
		using rvec3u32 = rvec3<u32>; ///< 3D row vectors of \ref u32.

		template <typename T> using rvec4 = row_vector<4, T>; ///< 4D row vectors.
		using rvec4f32 = rvec4<f32>; ///< 4D row vectors of \ref f32.
		using rvec4f64 = rvec4<f64>; ///< 4D row vectors of \ref f64.
		using rvec4i   = rvec4<int>; ///< 4D row vectors of \p int.
		using rvec4u32 = rvec4<u32>; ///< 4D row vectors of \ref u32.
	}
}
