#pragma once

/// \file
/// Matrices.

#include <cstddef>
#include <cassert>
#include <cmath>
#include <type_traits>
#include <algorithm>
#include <array>

#include "lotus/common.h"

namespace lotus {
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
		/// Initializes a vector.
		template <
			typename ...Args, typename Dummy = int,
			std::enable_if_t<sizeof...(Args) == dimensionality && (Rows == 1 || Cols == 1), Dummy> = 0
		> constexpr matrix(Args &&...data) : matrix(zero) {
			_fill_vector(*this, std::make_index_sequence<dimensionality>(), std::forward<Args>(data)...);
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
		template <typename ...Args> [[nodiscard]] inline static constexpr matrix diagonal(Args &&...args) {
			matrix result = zero;
			_fill_diagonal(result, std::make_index_sequence<std::min(Rows, Cols)>(), std::forward<Args>(args)...);
			return result;
		}

		/// Returns if any element of this matrix is \p NaN.
		[[nodiscard]] constexpr bool has_nan() const {
			for (std::size_t y = 0; y < num_rows; ++y) {
				for (std::size_t x = 0; x < num_columns; ++x) {
					if (std::isnan(elements[y][x])) {
						return true;
					}
				}
			}
			return false;
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

		/// Converts all elements into the specified type.
		template <typename U> [[nodiscard]] constexpr matrix<Cols, Rows, U> into() const {
			matrix<Cols, Rows, U> result = zero;
			for (std::size_t y = 0; y < Rows; ++y) {
				for (std::size_t x = 0; x < Cols; ++x) {
					result(y, x) = static_cast<U>(elements[y][x]);
				}
			}
			return result;
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

		/// Returns the inverse of this matrix.
		[[nodiscard]] constexpr matrix<Rows, Cols, T> inverse() const;

		/// Returns the trace of this matrix.
		[[nodiscard]] constexpr T trace() const {
			auto result = static_cast<T>(0);
			for (std::size_t i = 0; i < std::min(Rows, Cols); ++i) {
				result += elements[i][i];
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
			assert(row_start + RowCount <= Rows);
			assert(col_start + ColCount <= Cols);
			matrix<RowCount, ColCount, T> result = zero;
			for (std::size_t srcy = row_start, dsty = 0; dsty < RowCount; ++srcy, ++dsty) {
				for (std::size_t srcx = col_start, dstx = 0; dstx < ColCount; ++srcx, ++dstx) {
					result(dsty, dstx) = elements[srcy][srcx];
				}
			}
			return result;
		}

		/// Sets a submatrix to the given value.
		template <std::size_t RowCount, std::size_t ColCount> constexpr void set_block(
			std::size_t row_start, std::size_t col_start, matrix<RowCount, ColCount, T> mat
		) {
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
	protected:
		/// Fills this matrix as a vector.
		template <typename ...Args, std::size_t ...Is> constexpr static void _fill_vector(
			matrix &m, std::index_sequence<Is...>, Args &&...args
		) {
			static_assert(sizeof...(Args) == dimensionality, "incorrect number of dimensions");
			((m[Is] = std::forward<Args>(args)), ...);
		}
		/// Fills this matrix's diagonal.
		template <typename ...Args, std::size_t ...Is> constexpr static void _fill_diagonal(
			matrix &m, std::index_sequence<Is...>, Args &&...args
		) {
			static_assert(sizeof...(Args) == std::min(Rows, Cols), "incorrect number of diagonal entries");
			((m(Is, Is) = std::forward<Args>(args)), ...);
		}
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
		return lhs;
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
		return lhs;
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
		return m;
	}

	/// In-place scalar multiplication.
	template <
		std::size_t Rows, std::size_t Cols, typename T, typename U
	> inline constexpr matrix<Rows, Cols, T> &operator*=(matrix<Rows, Cols, T> &lhs, const U &rhs) {
		for (std::size_t y = 0; y < Rows; ++y) {
			for (std::size_t x = 0; x < Cols; ++x) {
				lhs(y, x) *= rhs;
			}
		}
		return lhs;
	}
	/// Scalar multiplication.
	template <
		std::size_t Rows, std::size_t Cols, typename T, typename U
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator*(matrix<Rows, Cols, T> lhs, const U &rhs) {
		lhs *= rhs;
		return lhs;
	}
	/// Scalar multiplication.
	template <
		std::size_t Rows, std::size_t Cols, typename T, typename U
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator*(const U &lhs, matrix<Rows, Cols, T> rhs) {
		rhs *= lhs;
		return rhs;
	}

	/// In-place scalar division.
	template <
		std::size_t Rows, std::size_t Cols, typename T, typename U
	> inline constexpr matrix<Rows, Cols, T> &operator/=(matrix<Rows, Cols, T> &lhs, const U &rhs) {
		for (std::size_t y = 0; y < Rows; ++y) {
			for (std::size_t x = 0; x < Cols; ++x) {
				lhs(y, x) /= rhs;
			}
		}
		return lhs;
	}
	/// Scalar division.
	template <
		std::size_t Rows, std::size_t Cols, typename T, typename U
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator/(matrix<Rows, Cols, T> lhs, const U &rhs) {
		lhs /= rhs;
		return lhs;
	}


	template <std::size_t N, typename T> class lup_decomposition;
	/// Matrix utilities.
	template <typename T> class mat {
	public:
		/// This class only contains utility functions.
		mat() = delete;

	protected:
		/// Returns the number of rows in the given matrix.
		template <typename Mat> constexpr static std::size_t _num_rows() {
			return std::decay_t<Mat>::num_rows;
		}
		/// Returns the number of columns in the given matrix.
		template <typename Mat> constexpr static std::size_t _num_cols() {
			return std::decay_t<Mat>::num_columns;
		}
		/// Implementation of \ref concat_columns() and \ref concat_rows().
		template <
			std::size_t Current, typename OutMat, typename CurMat, typename ...OtherMats
		> constexpr inline static void _concat_impl(OutMat &out, CurMat &&cur, OtherMats &&...others) {
			constexpr bool _is_rows = _num_cols<CurMat>() == _num_cols<OutMat>();

			out.set_block(
				_is_rows ? Current : 0,
				_is_rows ? 0 : Current,
				std::forward<CurMat>(cur)
			);
			if constexpr (sizeof...(OtherMats) > 0) {
				_concat_impl<Current + (_is_rows ? _num_rows<CurMat>() : _num_cols<CurMat>())>(
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
			_take<_num_rows<Mats>()...>(), _sum<_num_cols<Mats>()...>(), T
		> concat_columns(Mats &&...mats) {
			constexpr std::size_t
				_rows = _take<_num_rows<Mats>()...>(),
				_cols = _sum<_num_cols<Mats>()...>();
			using _mat = matrix<_rows, _cols, T>;

			static_assert(
				((_num_rows<Mats>() == _rows) && ...),
				"Arguments contain matrices with different numbers of rows"
			);
			_mat result = zero;
			_concat_impl<0>(result, std::forward<Mats>(mats)...);
			return result;
		}
		/// Creates a new matrix by concatenating the given matrices vertically.
		template <typename ...Mats> [[nodiscard]] inline static constexpr matrix<
			_sum<_num_rows<Mats>()...>(), _take<_num_cols<Mats>()...>(), T
		> concat_rows(Mats &&...mats) {
			constexpr std::size_t
				_rows = _sum<_num_rows<Mats>()...>(),
				_cols = _take<_num_cols<Mats>()...>();
			using _mat = matrix<_rows, _cols, T>;

			static_assert(
				((std::decay_t<Mats>::num_columns == _cols) && ...),
				"Arguments contain matrices with different numbers of columns"
			);
			_mat result = zero;
			_concat_impl<0>(result, std::forward<Mats>(mats)...);
			return result;
		}

		// products
		/// Kronecker product.
		template <
			std::size_t M1, std::size_t N1, std::size_t M2, std::size_t N2
		> constexpr static matrix<M1 * M2, N1 * N2, T> kronecker_product(
			const matrix<M1, N1, T> &lhs, const matrix<M2, N2, T> &rhs
		) {
			matrix<M1 * M2, N1 * N2, T> result = zero;
			std::size_t y = 0;
			for (std::size_t y1 = 0; y1 < M1; ++y1) {
				for (std::size_t y2 = 0; y2 < M2; ++y2, ++y) {
					std::size_t x = 0;
					for (std::size_t x1 = 0; x1 < N1; ++x1) {
						for (std::size_t x2 = 0; x2 < N2; ++x2, ++x) {
							result(y, x) = lhs(y1, x1) * rhs(y2, x2);
						}
					}
				}
			}
			return result;
		}

		/// Returns the inner product of the two matrices.
		template <typename Mat> [[nodiscard]] constexpr static typename Mat::value_type inner_product(
			const Mat &lhs, const Mat &rhs
		) {
			auto result = static_cast<typename Mat::value_type>(0);
			for (std::size_t y = 0; y < Mat::num_rows; ++y) {
				for (std::size_t x = 0; x < Mat::num_columns; ++x) {
					result += lhs(y, x) * rhs(y, x);
				}
			}
			return result;
		}

		// decomposition
		/// Shorthand for \ref lup_decomposition::compute().
		template <std::size_t N> [[nodiscard]] constexpr static lup_decomposition<N, T> lup_decompose(
			const matrix<N, N, T>&
		);

		// hax
		/// Computes the product of the two matrices, but only the upper-right triangle; then mirrors the upper-right
		/// triangle to the bottom-left triangle. This is used for accelerating matrix products that will produce
		/// symmetric matrices.
		template <
			std::size_t M, std::size_t N
		> [[nodiscard]] constexpr static matrix<M, M, T> multiply_into_symmetric(
			const matrix<M, N, T> &lhs, const matrix<N, M, T> &rhs
		) {
			matrix<M, M, T> result = zero;
			for (std::size_t y = 0; y < M; ++y) {
				for (std::size_t x = 0; x < y; ++x) {
					result(y, x) = result(x, y);
				}
				for (std::size_t x = y; x < M; ++x) {
					for (std::size_t k = 0; k < N; ++k) {
						result(y, x) += lhs(y, k) * rhs(k, x);
					}
				}
			}
			return result;
		}
	};


	/// LUP decomposition.
	///
	/// \sa https://en.wikipedia.org/wiki/LU_decomposition
	template <std::size_t N, typename T> class lup_decomposition {
	public:
		/// No initialization.
		lup_decomposition(uninitialized_t) {
		}

		/// Computes the decomposition of the given matrix.
		[[nodiscard]] constexpr static lup_decomposition compute(const matrix<N, N, T> &mat) {
			lup_decomposition res(mat);
			for (std::size_t x = 0; x < N; ++x) {
				T max = std::abs(res.result_rows(res.permutation[x], x));
				std::size_t max_id = x;
				for (std::size_t y = x + 1; y < N; ++y) {
					if (T cur = std::abs(res.result_rows(res.permutation[y], x)); cur > max) {
						max = cur;
						max_id = y;
					}
				}

				// TODO check degeneracy

				if (max_id != x) {
					std::swap(res.permutation[x], res.permutation[max_id]);
					++res.num_permutations;
				}

				for (std::size_t y = x + 1; y < N; ++y) {
					res.result_rows(res.permutation[y], x) /= res.result_rows(res.permutation[x], x);
					for (std::size_t k = x + 1; k < N; ++k) {
						res.result_rows(res.permutation[y], k) -=
							res.result_rows(res.permutation[y], x) * res.result_rows(res.permutation[x], k);
					}
				}
			}
			return res;
		}

		/// Inverts the matrix used to compute this decomposition.
		[[nodiscard]] constexpr matrix<N, N, T> invert() const {
			matrix<N, N, T> result = zero;
			for (std::size_t x = 0; x < N; ++x) {
				for (std::size_t y = 0; y < N; ++y) {
					if (permutation[y] == x) {
						result(y, x) = static_cast<T>(1);
					}
					for (std::size_t k = 0; k < y; ++k) {
						result(y, x) -= result_rows(permutation[y], k) * result(k, x);
					}
				}
				for (std::size_t y = N; y > 0; ) {
					--y;
					for (std::size_t k = y + 1; k < N; ++k) {
						result(y, x) -= result_rows(permutation[y], k) * result(k, x);
					}
					result(y, x) /= result_rows(permutation[y], y);
				}
			}
			return result;
		}

		/// Solves the linear system Ax = b where A is the matrix used to compute this decomposition.
		[[nodiscard]] constexpr matrix<N, 1, T> solve(const matrix<N, 1, T> &rhs) const {
			matrix<N, 1, T> result = zero;
			for (std::size_t i = 0; i < N; ++i) {
				result[i] = rhs[permutation[i]];
				for (std::size_t k = 0; k < i; ++k) {
					result[i] -= result_rows(permutation[i], k) * result[k];
				}
			}
			for (std::size_t i = N; i > 0; ) {
				--i;
				for (std::size_t k = i + 1; k < N; ++k) {
					result[i] -= result_rows(permutation[i], k) * result[k];
				}
				result[i] /= result_rows(permutation[i], i);
			}
			return result;
		}

		/// Computes the determinant of the original matrix.
		[[nodiscard]] constexpr double determinant() const {
			double det = result_rows(permutation[0], 0);
			for (std::size_t i = 1; i < N; ++i) {
				det *= result_rows(permutation[i], i);
			}
			return (num_permutations % 2 == 0) ? det : -det;
		}

		/// Rows of the decomposed matrices. The order of these rows are determined by \ref permutation. Its
		/// upper-right triangle as well as the diagonal stores the U matrix, and its lower-left triangle stores the
		/// L matrix without its diagonal. All elements on the diagonal of L are 1.
		matrix<N, N, T> result_rows = uninitialized;
		/// Permutation indices. This is the order of the rows in \ref result_rows.
		matrix<N, 1, std::size_t> permutation = uninitialized;
		std::size_t num_permutations; ///< Total permutation count, used for determinant computation.
	protected:
		/// Initializes this struct as the starting state of decomposition computation for the given matrix.
		/// Essentially initializes the result to the given matrix without permutation.
		constexpr explicit lup_decomposition(const matrix<N, N, T> &mat) :
			result_rows(mat), permutation(zero), num_permutations(0) {

			for (std::size_t i = 1; i < N; ++i) {
				permutation[i] = i;
			}
		}
	};


	/// A Gauss-Seidel solver.
	class gauss_seidel {
	public:
		/// Performs one Gauss-Seidel iteration. This function modifies the result vector in-place.
		template <std::size_t N, typename T> constexpr static void iterate(
			const matrix<N, N, T> &lhs, const matrix<N, 1, T> &rhs, matrix<N, 1, T> &result
		) {
			for (std::size_t i = 0; i < N; ++i) {
				result[i] = rhs[i];
				for (std::size_t j = 0; j < i; ++j) {
					result[i] -= lhs(i, j) * result[j];
				}
				for (std::size_t j = i + 1; j < N; ++j) {
					result[i] -= lhs(i, j) * result[j];
				}
				result[i] /= lhs(i, i);
			}
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


	// implementations
	template <
		std::size_t Rows, std::size_t Cols, typename T
	> constexpr matrix<Rows, Cols, T> matrix<Rows, Cols, T>::inverse() const {
		return lup_decomposition<Cols, T>::compute(*this).invert();
	}


	template <typename T> template <std::size_t N> constexpr lup_decomposition<N, T> mat<T>::lup_decompose(
		const matrix<N, N, T> &mat
	) {
		return lup_decomposition<N, T>::compute(mat);
	}
}
