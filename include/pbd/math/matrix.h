#pragma once

/// \file
/// Matrices.

#include <cstddef>
#include <cassert>
#include <cmath>
#include <type_traits>
#include <algorithm>
#include <array>

namespace pbd {
	/// A <tt>Rows x Cols</tt> matrix.
	template <std::size_t Rows, std::size_t Cols, typename T> struct matrix {
	public:
		constexpr static std::size_t
			num_rows = Rows, ///< The number of rows.
			num_columns = Cols, ///< The number of columns.
			dimensionality = std::max(Rows, Cols); ///< Maximum of \ref num_rows and \ref num_columns.
		using value_type = T; ///< Value type.

		/// Initializes the entire matrix.
		constexpr explicit matrix(std::initializer_list<std::initializer_list<T>> data) : elements{} {
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

		/// Returns an uninitialized matrix.
		[[nodiscard]] inline static matrix uninitialized() {
			return matrix();
		}
		/// Returns a zero matrix.
		[[nodiscard]] inline static constexpr matrix zero() {
			return matrix{ _zero_init() };
		}
		/// Returns an identity matrix.
		[[nodiscard]] inline static constexpr matrix identity() {
			matrix result = zero();
			for (std::size_t i = 0; i < std::min(Rows, Cols); ++i) {
				result(i, i) = static_cast<T>(1);
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
			auto result = matrix<Cols, Rows, T>::zero();
			for (std::size_t y = 0; y < Rows; ++y) {
				for (std::size_t x = 0; x < Cols; ++x) {
					result(x, y) = elements[y][x];
				}
			}
			return result;
		}

		/// Returns the i-th row.
		[[nodiscard]] constexpr matrix<1, Cols, T> row(std::size_t r) const {
			auto result = matrix<1, Cols, T>::zero();
			result.elements[0] = elements[r];
			return result;
		}
		/// Returns the i-th column.
		[[nodiscard]] constexpr matrix<Rows, 1, T> column(std::size_t c) const {
			auto result = matrix<Rows, 1, T>::zero();
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
			auto result = matrix<RowCount, ColCount, T>::zero();
			for (std::size_t srcy = row_start, dsty = 0; dsty < RowCount; ++srcy, ++dsty) {
				for (std::size_t srcx = col_start, dstx = 0; dstx < ColCount; ++srcx, ++dstx) {
					result(dsty, dstx) = elements[srcy][srcx];
				}
			}
			return result;
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
		/// Used for zero-initializing this matrix.
		struct _zero_init {
		};

		/// Does not initialize the matrix.
		matrix() = default;
		/// Value-initializes (zero-initialize for primitive types) this matrix.
		constexpr explicit matrix(_zero_init) : elements{} {
		}
	};


	// arithmetic
	/// Matrix multiplication.
	template <
		std::size_t Rows, std::size_t Col1Row2, std::size_t Cols, typename T
	> [[nodiscard]] inline constexpr matrix<Rows, Cols, T> operator*(
		const matrix<Rows, Col1Row2, T> &lhs, const matrix<Col1Row2, Cols, T> &rhs
	) {
		auto result = matrix<Rows, Cols, T>::zero();
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
}
