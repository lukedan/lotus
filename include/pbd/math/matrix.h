#pragma once

/// \file
/// Matrices.

#include <cstddef>
#include <cassert>
#include <cmath>
#include <type_traits>
#include <algorithm>
#include <array>

#include "../common.h"

namespace pbd {
	/// A <tt>Rows x Cols</tt> matrix.
	template <std::size_t Rows, std::size_t Cols, typename T> struct matrix {
	public:
		static_assert(Rows > 0 && Cols > 0, "Matrices with zero dimensions are invalid");

		constexpr static std::size_t
			num_rows = Rows, ///< The number of rows.
			num_columns = Cols, ///< The number of columns.
			dimensionality = std::max(Rows, Cols); ///< Maximum of \ref num_rows and \ref num_columns.
		using value_type = T; ///< Value type.

		/// Does not initialize the matrix.
		matrix(uninitialized_t) {
		}
		/// Value-initializes (zero-initialize for primitive types) this matrix.
		constexpr matrix(zero_t) : elements{} {
		}
		/// Initializes the entire matrix.
		constexpr matrix(std::initializer_list<std::initializer_list<T>> data) : elements{} {
			assert(data.size() == Rows);
			for (auto row_it = data.begin(); row_it != data.end(); ++row_it) {
				assert(row_it->size() == Cols);
				for (auto col_it = row_it->begin(); col_it != row_it->end(); ++col_it) {
					elements[row_it - data.begin()][col_it - row_it->begin()] = std::move(*col_it);
				}
			}
		}
		/// Default move constructor.
		constexpr matrix(matrix&&) = default;
		/// Default copy constructor.
		constexpr matrix(const matrix&) = default;
		/// Default move assignment.
		constexpr matrix &operator=(matrix&&) = default;
		/// Default copy assignment.
		constexpr matrix &operator=(const matrix&) = default;

		/// Returns an identity matrix.
		[[nodiscard]] inline static constexpr matrix identity() {
			matrix result = zero;
			for (std::size_t i = 0; i < std::min(Rows, Cols); ++i) {
				result(i, i) = static_cast<T>(1);
			}
			return result;
		}
		/// Returns a diagonal matrix with the given values on its diagonal.
		[[nodiscard]] inline static constexpr matrix diagonal(std::initializer_list<T> diag) {
			assert(diag.size() == std::min(Rows, Cols));
			matrix result = zero;
			std::size_t i = 0;
			for (auto it = diag.begin(); it != diag.end(); ++i, ++it) {
				result(i, i) = std::move(*it);
			}
			return result;
		}

		/// Indexing.
		[[nodiscard]] constexpr T &operator()(std::size_t row, std::size_t col) {
			return elements[row][col];
		}
		/// Indexing.
		[[nodiscard]] constexpr const T &operator()(std::size_t row, std::size_t col) const {
			return elements[row][col];
		}
		/// Vector indexing - only valid for matrices with one of its dimensions being 1.
		template <
			typename Dummy = int, std::enable_if_t<Rows == 1 || Cols == 1, Dummy> = 0
		> [[nodiscard]] constexpr T &operator[](std::size_t i) {
			if constexpr (Rows == 1) {
				return elements[0][i];
			} else {
				return elements[i][0];
			}
		}
		/// \overload
		template <
			typename Dummy = int, std::enable_if_t<Rows == 1 || Cols == 1, Dummy> = 0
		> [[nodiscard]] constexpr const T &operator[](std::size_t i) const {
			if constexpr (Rows == 1) {
				return elements[0][i];
			} else {
				return elements[i][0];
			}
		}

		/// Returns the transposed matrix.
		[[nodiscard]] constexpr matrix<Cols, Rows, T> transposed() const {
			matrix<Cols, Rows, T> result = zero;
			for (std::size_t y = 0; y < Rows; ++y) {
				for (std::size_t x = 0; x < Cols; ++x) {
					result(x, y) = elements[y][x];
				}
			}
			return result;
		}

		/// Returns the i-th row.
		[[nodiscard]] constexpr matrix<1, Cols, T> row(std::size_t r) const {
			matrix<1, Cols, T> result = zero;
			result.elements[0] = elements[r];
			return result;
		}
		/// Returns the i-th column.
		[[nodiscard]] constexpr matrix<Rows, 1, T> column(std::size_t c) const {
			matrix<Rows, 1, T> result = zero;
			for (std::size_t i = 0; i < Rows; ++i) {
				result(i, 0) = elements[i][c];
			}
			return result;
		}
		/// Returns a submatrix.
		template <
			std::size_t RowCount, std::size_t ColCount
		> [[nodiscard]] constexpr matrix<RowCount, ColCount, T> block(
			std::size_t row_start, std::size_t col_start
		) const {
			matrix<RowCount, ColCount, T> result = zero;
			for (std::size_t srcy = row_start, dsty = 0; dsty < RowCount; ++srcy, ++dsty) {
				for (std::size_t srcx = col_start, dstx = 0; dstx < ColCount; ++srcx, ++dstx) {
					result(dsty, dstx) = elements[srcy][srcx];
				}
			}
			return result;
		}

		/// Sets a submatrix to the given value.
		template <
			std::size_t RowCount, std::size_t ColCount
		> constexpr void set_block(std::size_t row_start, std::size_t col_start, matrix<RowCount, ColCount, T> mat) {
			assert(row_start + RowCount <= Rows);
			assert(col_start + ColCount <= Cols);
			for (std::size_t srcy = 0, dsty = row_start; srcy < RowCount; ++srcy, ++dsty) {
				for (std::size_t srcx = 0, dstx = col_start; srcx < ColCount; ++srcx, ++dstx) {
					elements[dsty][dstx] = std::move(mat(srcy, srcx));
				}
			}
		}


		/// Computes the squared Frobenius norm of this matrix.
		[[nodiscard]] constexpr T squared_norm() const {
			auto result = static_cast<T>(0);
			for (std::size_t y = 0; y < Rows; ++y) {
				for (std::size_t x = 0; x < Cols; ++x) {
					result += elements[y][x] * elements[y][x];
				}
			}
			return result;
		}
		/// Squared root of \ref squared_norm().
		template <
			typename Res = std::conditional_t<std::is_floating_point_v<T>, T, double>
		> [[nodiscard]] constexpr Res norm() const {
			return std::sqrt(static_cast<Res>(squared_norm()));
		}

		std::array<std::array<T, Cols>, Rows> elements; ///< The elements of this matrix.
	};


	// arithmetic
	/// Matrix multiplication.
	template <
		std::size_t Rows, std::size_t Col1Row2, std::size_t Cols, typename T
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator*(
		const matrix<Rows, Col1Row2, T> &lhs, const matrix<Col1Row2, Cols, T> &rhs
	) {
		matrix<Rows, Cols, T> result = zero;
		for (std::size_t y = 0; y < Rows; ++y) {
			for (std::size_t x = 0; x < Cols; ++x) {
				T &res = result(y, x);
				for (std::size_t k = 0; k < Col1Row2; ++k) {
					res += lhs(y, k) * rhs(k, x);
				}
			}
		}
		return result;
	}

	/// In-place memberwise addition.
	template <std::size_t Rows, std::size_t Cols, typename T> inline constexpr matrix<Rows, Cols, T> &operator+=(
		matrix<Rows, Cols, T> &lhs, const matrix<Rows, Cols, T> &rhs
	) {
		for (std::size_t y = 0; y < Rows; ++y) {
			for (std::size_t x = 0; x < Cols; ++x) {
				lhs(y, x) += rhs(y, x);
			}
		}
		return lhs;
	}
	/// Memberwise addition.
	template <
		std::size_t Rows, std::size_t Cols, typename T
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator+(
		matrix<Rows, Cols, T> lhs, const matrix<Rows, Cols, T> &rhs
	) {
		lhs += rhs;
		return std::move(lhs);
	}

	/// In-place memberwise subtraction.
	template <std::size_t Rows, std::size_t Cols, typename T> inline constexpr matrix<Rows, Cols, T> &operator-=(
		matrix<Rows, Cols, T> &lhs, const matrix<Rows, Cols, T> &rhs
	) {
		for (std::size_t y = 0; y < Rows; ++y) {
			for (std::size_t x = 0; x < Cols; ++x) {
				lhs(y, x) -= rhs(y, x);
			}
		}
		return lhs;
	}
	/// Memberwise subtraction.
	template <
		std::size_t Rows, std::size_t Cols, typename T
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator-(
		matrix<Rows, Cols, T> lhs, const matrix<Rows, Cols, T> &rhs
	) {
		lhs -= rhs;
		return std::move(lhs);
	}
	/// Negation.
	template <
		std::size_t Rows, std::size_t Cols, typename T
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator-(matrix<Rows, Cols, T> m) {
		for (std::size_t y = 0; y < Rows; ++y) {
			for (std::size_t x = 0; x < Cols; ++x) {
				m(y, x) = -m(y, x);
			}
		}
		return std::move(m);
	}

	/// In-place scalar multiplication.
	template <std::size_t Rows, std::size_t Cols, typename T> inline constexpr matrix<Rows, Cols, T> &operator*=(
		matrix<Rows, Cols, T> &lhs, const T &rhs
	) {
		for (std::size_t y = 0; y < Rows; ++y) {
			for (std::size_t x = 0; x < Cols; ++x) {
				lhs(y, x) *= rhs;
			}
		}
		return lhs;
	}
	/// Scalar multiplication.
	template <
		std::size_t Rows, std::size_t Cols, typename T
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator*(matrix<Rows, Cols, T> lhs, const T &rhs) {
		lhs *= rhs;
		return std::move(lhs);
	}
	/// Scalar multiplication.
	template <
		std::size_t Rows, std::size_t Cols, typename T
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator*(const T &lhs, matrix<Rows, Cols, T> rhs) {
		rhs *= lhs;
		return std::move(rhs);
	}

	/// In-place scalar division.
	template <
		std::size_t Rows, std::size_t Cols, typename T
	> inline constexpr matrix<Rows, Cols, T> &operator/=(matrix<Rows, Cols, T> &lhs, const T &rhs) {
		for (std::size_t y = 0; y < Rows; ++y) {
			for (std::size_t x = 0; x < Cols; ++x) {
				lhs(y, x) /= rhs;
			}
		}
		return lhs;
	}
	/// Scalar division.
	template <
		std::size_t Rows, std::size_t Cols, typename T
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator/(matrix<Rows, Cols, T> lhs, const T &rhs) {
		lhs /= rhs;
		return std::move(lhs);
	}


	/// Matrix utilities.
	template <typename T> class mat {
	public:
		/// This class only contains utility functions.
		mat() = delete;

	protected:
		/// Implementation of \ref concat_columns() and \ref concat_rows().
		template <
			std::size_t Current, typename OutMat, typename CurMat, typename ...OtherMats
		> inline static void _concat_impl(OutMat &out, CurMat &&cur, OtherMats &&...others) {
			constexpr bool _is_rows = CurMat::num_columns == OutMat::num_columns;

			out.set_block<CurMat::num_rows, CurMat::num_columns>(
				_is_rows ? Current : 0,
				_is_rows ? 0 : Current,
				std::forward<CurMat>(cur)
			);
			if constexpr (sizeof...(OtherMats) > 0) {
				_concat_impl<Current + (_is_rows ? CurMat::num_rows : CurMat::num_columns)>(
					out, std::forward<OtherMats>(others)...
				);
			}
		}
		/// Returns the sum of all inputs.
		template <std::size_t ...Is> [[nodiscard]] constexpr inline static std::size_t _sum() {
			return (Is + ...);
		}
		/// Returns the first input.
		template <std::size_t I, std::size_t...> [[nodiscard]] constexpr inline static std::size_t _take() {
			return I;
		}
	public:
		/// Creates a new matrix by concatenating the given matrices horizontally.
		template <typename ...Mats> [[nodiscard]] inline static constexpr matrix<
			_take<Mats::num_rows...>(), _sum<Mats::num_columns...>(), T
		> concat_columns(Mats &&...mats) {
			constexpr std::size_t
				_rows = _take<Mats::num_rows...>(),
				_cols = _sum<Mats::num_columns...>();
			using _mat = matrix<_rows, _cols, T>;

			static_assert(
				((Mats::num_rows == _rows) && ...),
				"Arguments contain matrices with different numbers of rows"
			);
			_mat result = zero;
			_concat_impl<0>(result, std::forward<Mats>(mats)...);
			return result;
		}
		/// Creates a new matrix by concatenating the given matrices vertically.
		template <typename ...Mats> [[nodiscard]] inline static constexpr matrix<
			_sum<Mats::num_rows...>(), _take<Mats::num_columns...>(), T
		> concat_rows(Mats &&...mats) {
			constexpr std::size_t
				_rows = _sum<Mats::num_rows...>(),
				_cols = _take<Mats::num_columns...>();
			using _mat = matrix<_rows, _cols, T>;

			static_assert(
				((Mats::num_columns == _cols) && ...),
				"Arguments contain matrices with different numbers of columns"
			);
			_mat result = zero;
			_concat_impl<0>(result, std::forward<Mats>(mats)...);
			return result;
		}
	};


	// typedefs
	template <typename T> using mat22 = matrix<2, 2, T>; ///< 2x2 matrices.
	using mat22f = matrix<2, 2, float>; ///< 2x2 matrices of \p float.
	using mat22d = matrix<2, 2, double>; ///< 2x2 matrices of \p double.

	template <typename T> using mat23 = matrix<2, 3, T>; ///< 2x3 matrices.
	using mat23f = matrix<2, 3, float>; ///< 2x2 matrices of \p float.
	using mat23d = matrix<2, 3, double>; ///< 2x2 matrices of \p double.

	template <typename T> using mat33 = matrix<3, 3, T>; ///< 3x3 matrices.
	using mat33f = matrix<3, 3, float>; ///< 2x2 matrices of \p float.
	using mat33d = matrix<3, 3, double>; ///< 2x2 matrices of \p double.

	template <typename T> using mat34 = matrix<3, 4, T>; ///< 3x4 matrices.
	using mat34f = matrix<3, 4, float>; ///< 2x2 matrices of \p float.
	using mat34d = matrix<3, 4, double>; ///< 2x2 matrices of \p double.

	template <typename T> using mat44 = matrix<4, 4, T>; ///< 4x4 matrices.
	using mat44f = matrix<4, 4, float>; ///< 2x2 matrices of \p float.
	using mat44d = matrix<4, 4, double>; ///< 2x2 matrices of \p double.

	using matf = mat<float>; ///< Utilities for matrices of \p float.
	using matd = mat<double>; ///< Utilities for matrices of \p double.
}
