#pragma once

/// \file
/// Matrices.

#include <cmath>
#include <type_traits>
#include <algorithm>
#include <array>
#include <format>

#include "lotus/common.h"
#include "numeric_traits.h"

namespace lotus {
	template <usize Rows, usize Cols, typename T> struct matrix;

	namespace _details {
		/// Used to determine if the type is a matrix type.
		template <typename> struct is_matrix : std::false_type {
		};
		/// Specialization for matrix types.
		template <
			usize Rows, usize Cols, typename U
		> struct is_matrix<matrix<Rows, Cols, U>> : std::true_type {
		};
		template <typename T> constexpr bool is_matrix_v = is_matrix<T>::value; ///< Shorthand for \ref is_matrix.
		template <typename T> concept matrix_type = is_matrix_v<T>; ///< Matrix concept.

		/// Used to determine if the type is a vector type.
		template <typename> struct is_vector : std::false_type {
		};
		/// Vector types have 1 column or 1 row.
		template <usize Rows, usize Cols, typename T> struct is_vector<matrix<Rows, Cols, T>> :
			std::integral_constant<bool, Rows == 1 || Cols == 1> {
		};
		template <typename T> constexpr bool is_vector_v = is_vector<T>::value; ///< Shorthand for \ref is_vector.
		template <typename T> concept vector_type = is_vector_v<T>; ///< Vector concept.


		/// Helper class used for obtaining the number of rows and columns in a matrix.
		template <typename> struct matrix_size {
			/// Scalars are one-dimensional.
			constexpr static usize width = 1;
			/// Scalars are one-dimensional.
			constexpr static usize height = 1;
		};
		/// Specialization for matrix types.
		template <usize Rows, usize Cols, typename T> struct matrix_size<matrix<Rows, Cols, T>> {
			constexpr static usize width = Cols; ///< Width of the matrix.
			constexpr static usize height = Rows; ///< Height of the matrix.
		};
		/// Shorthand for matrix width.
		template <typename T> constexpr usize matrix_width_v = matrix_size<T>::width;
		/// Shorthand for matrix height.
		template <typename T> constexpr usize matrix_height_v = matrix_size<T>::height;


		template <typename...> struct first_value_type;
		/// Returns the first available \ref matrix::value_type.
		template <typename FirstMat, typename ...Mats> struct first_value_type<FirstMat, Mats...> {
			using type = std::conditional_t<
				std::is_same_v<std::decay_t<FirstMat>, zero_t>,
				typename first_value_type<Mats...>::type,
				typename std::decay_t<FirstMat>::value_type
			>; ///< Type that ignores \ref zero_t.
		};
		/// Specialization for invalid types.
		template <> struct first_value_type<> {
			using type = void; ///< Invalid data type.
		};
		template <typename ...Mats> using first_value_type_t = first_value_type<Mats...>::type;
	}

	/// A <tt>Rows x Cols</tt> matrix.
	template <usize Rows, usize Cols, typename T> struct matrix {
	public:
		static_assert(Rows > 0 && Cols > 0, "Matrices with zero dimensions are invalid");

		constexpr static usize num_rows = Rows; ///< The number of rows.
		constexpr static usize num_columns = Cols; ///< The number of columns.
		/// Maximum of \ref num_rows and \ref num_columns.
		constexpr static usize dimensionality = std::max(num_rows, num_columns);
		using value_type = T; ///< Value type.

		/// Value initialization when no args are present.
		constexpr matrix() : elements{} {
		}
		/// Does not initialize the matrix.
		matrix(uninitialized_t) {
		}
		/// Value-initializes (zero-initialize for primitive types) this matrix.
		constexpr matrix(zero_t) : elements{} {
		}
		/// Initializes the entire matrix.
		constexpr matrix(std::initializer_list<std::initializer_list<T>> data) : elements{} {
			crash_if(data.size() != Rows);
			for (auto row_it = data.begin(); row_it != data.end(); ++row_it) {
				crash_if(row_it->size() != Cols);
				const auto r = static_cast<usize>(row_it - data.begin());
				for (auto col_it = row_it->begin(); col_it != row_it->end(); ++col_it) {
					const auto c = static_cast<usize>(col_it - row_it->begin());
					elements[r][c] = std::move(*col_it);
				}
			}
		}
		/// Initializes a vector.
		template <typename ...Args> constexpr matrix(Args &&...data) requires (
			sizeof...(Args) > 1 && (
				(Rows == 1 && ((_details::matrix_height_v<Args> == 1) && ...)) ||
				(Cols == 1 && ((_details::matrix_width_v<Args> == 1) && ...))
			)
		) : elements{} {
			_set_vector<0>(*this, std::forward<Args>(data)...);
		}
		/// Initializes a vector from an array.
		constexpr matrix(const T (&arr)[dimensionality]) requires _details::vector_type<matrix> {
			for (usize i = 0; i < dimensionality; ++i) {
				operator[](i) = arr[i];
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
		[[nodiscard]] static constexpr matrix identity() {
			matrix result = zero;
			for (usize i = 0; i < std::min(Rows, Cols); ++i) {
				result(i, i) = static_cast<T>(1);
			}
			return result;
		}
		/// Returns a diagonal matrix with the values in the given vector on its diagonal.
		[[nodiscard]] static constexpr matrix diagonal(matrix<std::min(Rows, Cols), 1, T> values) {
			matrix result = zero;
			for (usize i = 0; i < std::min(Rows, Cols); ++i) {
				result(i, i) = std::move(values[i]);
			}
			return result;
		}
		/// \overload
		[[nodiscard]] static constexpr matrix diagonal(
			matrix<1, std::min(Rows, Cols), T> values
		) requires (std::min(Rows, Cols) > 1) {
			matrix result = zero;
			for (usize i = 0; i < std::min(Rows, Cols); ++i) {
				result(i, i) = std::move(values[i]);
			}
			return result;
		}
		/// Shorthand for constructing a new vector and calling \ref diagonal() with it.
		template <typename ...Args> [[nodiscard]] static constexpr matrix diagonal(
			Args &&...args
		) requires (sizeof...(Args) > 1) {
			return diagonal(matrix<std::min(Rows, Cols), 1, T>(std::forward<Args>(args)...));
		}
		/// Returns a matrix filled with the given value.
		[[nodiscard]] static constexpr matrix filled(const T &val) {
			matrix result = zero;
			for (usize y = 0; y < Rows; ++y) {
				for (usize x = 0; x < Cols; ++x) {
					result(y, x) = val;
				}
			}
			return result;
		}

		/// Returns if any element of this matrix is \p NaN.
		[[nodiscard]] constexpr bool has_nan() const {
			for (usize y = 0; y < num_rows; ++y) {
				for (usize x = 0; x < num_columns; ++x) {
					if (std::isnan(elements[y][x])) {
						return true;
					}
				}
			}
			return false;
		}

		/// Indexing.
		[[nodiscard]] constexpr T &operator()(usize row, usize col) {
			return elements[row][col];
		}
		/// Indexing.
		[[nodiscard]] constexpr const T &operator()(usize row, usize col) const {
			return elements[row][col];
		}
		/// Vector indexing - only valid for matrices with one of its dimensions being 1.
		[[nodiscard]] constexpr T &operator[](usize i) requires _details::vector_type<matrix> {
			if constexpr (Rows == 1) {
				return elements[0][i];
			} else {
				return elements[i][0];
			}
		}
		/// \overload
		[[nodiscard]] constexpr const T &operator[](usize i) const requires _details::vector_type<matrix> {
			if constexpr (Rows == 1) {
				return elements[0][i];
			} else {
				return elements[i][0];
			}
		}

		/// Converts all elements into the specified type.
		template <typename U> [[nodiscard]] constexpr matrix<Rows, Cols, U> into() const {
			matrix<Rows, Cols, U> result = zero;
			for (usize y = 0; y < Rows; ++y) {
				for (usize x = 0; x < Cols; ++x) {
					result(y, x) = static_cast<U>(elements[y][x]);
				}
			}
			return result;
		}

		/// Returns the transposed matrix.
		[[nodiscard]] constexpr matrix<Cols, Rows, T> transposed() const {
			matrix<Cols, Rows, T> result = zero;
			for (usize y = 0; y < Rows; ++y) {
				for (usize x = 0; x < Cols; ++x) {
					result(x, y) = elements[y][x];
				}
			}
			return result;
		}

		/// Computes the inverse of this matrix.
		[[nodiscard]] constexpr matrix<Rows, Cols, T> inverse() const;
		/// Computes the determinant of this matrix.
		template <typename U = T> [[nodiscard]] constexpr U determinant() const requires (Rows == Cols);

		/// Returns the trace of this matrix.
		[[nodiscard]] constexpr T trace() const {
			auto result = static_cast<T>(0);
			for (usize i = 0; i < std::min(Rows, Cols); ++i) {
				result += elements[i][i];
			}
			return result;
		}

		/// Returns the i-th row.
		[[nodiscard]] constexpr matrix<1, Cols, T> row(usize r) const {
			matrix<1, Cols, T> result = zero;
			result.elements[0] = elements[r];
			return result;
		}
		/// Returns the i-th column.
		[[nodiscard]] constexpr matrix<Rows, 1, T> column(usize c) const {
			matrix<Rows, 1, T> result = zero;
			for (usize i = 0; i < Rows; ++i) {
				result(i, 0) = elements[i][c];
			}
			return result;
		}
		/// Returns a submatrix.
		template <
			usize RowCount, usize ColCount
		> [[nodiscard]] constexpr matrix<RowCount, ColCount, T> block(
			usize row_start, usize col_start
		) const {
			crash_if(row_start + RowCount > Rows);
			crash_if(col_start + ColCount > Cols);
			matrix<RowCount, ColCount, T> result = zero;
			for (usize srcy = row_start, dsty = 0; dsty < RowCount; ++srcy, ++dsty) {
				for (usize srcx = col_start, dstx = 0; dstx < ColCount; ++srcx, ++dstx) {
					result(dsty, dstx) = elements[srcy][srcx];
				}
			}
			return result;
		}
		/// Returns a subvector.
		template <usize Size> [[nodiscard]] constexpr auto subvector(
			usize components
		) const requires _details::vector_type<matrix> {
			if constexpr (Rows == 1) {
				return block<1, Size>(0, components);
			} else {
				return block<Size, 1>(components, 0);
			}
		}

		/// Sets a submatrix to the given value.
		template <usize RowCount, usize ColCount> constexpr void set_block(
			usize row_start, usize col_start, matrix<RowCount, ColCount, T> mat
		) requires (RowCount <= Rows && ColCount <= Cols) {
			crash_if(row_start + RowCount > Rows);
			crash_if(col_start + ColCount > Cols);
			for (usize srcy = 0, dsty = row_start; srcy < RowCount; ++srcy, ++dsty) {
				for (usize srcx = 0, dstx = col_start; srcx < ColCount; ++srcx, ++dstx) {
					elements[dsty][dstx] = std::move(mat(srcy, srcx));
				}
			}
		}
		/// Sets a subvector to the given value.
		template <usize OtherRows, usize OtherCols> constexpr void set_subvector(
			usize start, matrix<OtherRows, OtherCols, T> vec
		) requires _details::vector_type<matrix> {
			if constexpr (Rows == 1) {
				set_block(0, start, std::move(vec));
			} else {
				set_block(start, 0, std::move(vec));
			}
		}


		/// Computes the squared Frobenius norm of this matrix.
		[[nodiscard]] constexpr T squared_norm() const {
			auto result = static_cast<T>(0);
			for (usize y = 0; y < Rows; ++y) {
				for (usize x = 0; x < Cols; ++x) {
					result += elements[y][x] * elements[y][x];
				}
			}
			return result;
		}
		/// Squared root of \ref squared_norm().
		[[nodiscard]] constexpr T norm() const {
			return numeric_traits<T>::sqrt(squared_norm());
		}

		/// Returns \p true if all elements of this matrix are true.
		[[nodiscard]] constexpr bool all_true() const requires std::is_same_v<T, bool> {
			for (const auto &row : elements) {
				for (const bool elem : row) {
					if (!elem) {
						return false;
					}
				}
			}
			return true;
		}
		/// Returns \p true if any element of this matrix is true.
		[[nodiscard]] constexpr bool any_true() const requires std::is_same_v<T, bool> {
			for (const auto &row : elements) {
				for (const bool elem : row) {
					if (elem) {
						return true;
					}
				}
			}
			return false;
		}

		/// Default equality and inequality comparisons.
		[[nodiscard]] friend bool operator==(const matrix&, const matrix&) = default;

		std::array<std::array<T, Cols>, Rows> elements; ///< The elements of this matrix.
	protected:
		/// End of recursion.
		template <usize Index> constexpr static void _set_vector(matrix&) {
			static_assert(Index == dimensionality, "Incorrect number of components");
		}
		/// Fills several components of the vector.
		template <
			usize Index, usize MyRows, usize MyCols, typename U, typename ...OtherArgs
		> constexpr static void _set_vector(
			matrix &m, const matrix<MyRows, MyCols, U> &first, OtherArgs &&...other_args
		) {
			constexpr usize _current_dimensionality = std::max(MyRows, MyCols);
			for (usize i = 0; i < _current_dimensionality; ++i) {
				m[i + Index] = first[i];
			}
			_set_vector<Index + _current_dimensionality>(m, std::forward<OtherArgs>(other_args)...);
		}
		/// Fills one component of the vector.
		template <usize Index, typename U, typename ...OtherArgs> constexpr static void _set_vector(
			matrix &m, U &&first, OtherArgs &&...other_args
		) requires (!_details::is_matrix_v<std::decay_t<U>>) {
			m[Index] = std::forward<U>(first);
			_set_vector<Index + 1>(m, std::forward<OtherArgs>(other_args)...);
		}
	};
}
/// Formatter for matrices.
template <
	lotus::usize Rows, lotus::usize Cols, typename T
> struct std::formatter<lotus::matrix<Rows, Cols, T>, char> {
	/// Saves the format string.
	template <typename ParseCtx> constexpr ParseCtx::iterator parse(ParseCtx &ctx) {
		typename ParseCtx::iterator it = ctx.begin();
		_fmt_string += '{';
		for (; it != ctx.end() && *it != '}'; ++it) {
			_fmt_string += *it;
		}
		_fmt_string += '}';
		return it;
	}

	/// Formats the given matrix.
	template <typename FormatCtx> FormatCtx::iterator format(
		const lotus::matrix<Rows, Cols, T> &mat, FormatCtx &ctx
	) const {
		if constexpr (Rows == 1 || Cols == 1) {
			auto out = ctx.out();
			out = std::format_to(std::move(out), "{{");
			out = _print_one(std::move(out), mat[0]);
			for (size_t i = 1; i < max(Rows, Cols); ++i) {
				out = std::format_to(std::move(out), ", ");
				out = _print_one(std::move(out), mat[i]);
			}
			return std::format_to(std::move(out), "}}");
		} else {
			auto out = ctx.out();
			out = std::format_to(std::move(out), "{{");
			for (size_t row = 0; row < Rows; ++row) {
				out = std::format_to(std::move(out), "{}", row == 0 ? "{" : ", {");
				for (size_t col = 0; col < Cols; ++col) {
					if (col != 0) {
						out = std::format_to(std::move(out), ", ");
					}
					out = _print_one(std::move(out), mat(row, col));
				}
				out = std::format_to(std::move(out), "}}");
			}
			return std::format_to(std::move(out), "}}");
		}
	}
private:
	std::string _fmt_string; ///< Original formatting args.

	/// Prints a single element to the output.
	template <typename It> It _print_one(It &&out, const T &val) const {
		return std::vformat_to(std::move(out), _fmt_string, std::make_format_args(val));
	}
};

namespace lotus {
	/// Memberwise matrix utilities.
	class mat_memberwise {
	public:
		/// This class only contains utility functions.
		mat_memberwise() = delete;

		/// Applies the memberwise operator to the given matrices and returns the result.
		template <
			usize R, usize C, typename ...Types, typename Operator
		> [[nodiscard]] constexpr static std::conditional_t<
			std::is_same_v<std::invoke_result_t<Operator, const Types&...>, void>,
			void,
			matrix<R, C, std::invoke_result_t<Operator, const Types&...>>
		> operation(
			Operator op, const matrix<R, C, Types> &...mats
		) {
			using _result_type = std::invoke_result_t<Operator, const Types&...>;
			if constexpr (std::is_same_v<_result_type, void>) {
				for (usize y = 0; y < R; ++y) {
					for (usize x = 0; x < C; ++x) {
						op(mats(y, x)...);
					}
				}
			} else {
				matrix<R, C, _result_type> result = zero;
				for (usize y = 0; y < R; ++y) {
					for (usize x = 0; x < C; ++x) {
						result(y, x) = op(mats(y, x)...);
					}
				}
				return result;
			}
		}
		/// Memberwise multiplication of the two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, T> multiply(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return lhs * rhs;
			}, lhs, rhs);
		}
		/// Memberwise multiplication of the two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, T> divide(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return lhs / rhs;
			}, lhs, rhs);
		}
		/// Memberwise minimum of the two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, T> min(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return std::min(lhs, rhs);
			}, lhs, rhs);
		}
		/// Memberwise minimum of the two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, T> max(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return std::max(lhs, rhs);
			}, lhs, rhs);
		}
		/// Memberwise reciprocal of the given matrix.
		template <
			usize R, usize C, typename T
		> [[nodiscard]] constexpr static matrix<R, C, T> reciprocal(const matrix<R, C, T> &m) {
			return operation([](const T &v) {
				return static_cast<T>(1.0f / v);
			}, m);
		}
		/// Memberwise absolute value.
		template <
			usize R, usize C, typename T
		> [[nodiscard]] constexpr static matrix<R, C, T> abs(const matrix<R, C, T> &m) {
			return operation([](const T &val) {
				return std::abs(val);
			}, m);
		}

		/// Memberwise equality of two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, bool> equals(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return lhs == rhs;
			}, lhs, rhs);
		}
		/// Memberwise comparison of two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, bool> greater(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return lhs > rhs;
			}, lhs, rhs);
		}
		/// Memberwise comparison of two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, bool> less(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return lhs < rhs;
			}, lhs, rhs);
		}
		/// Memberwise comparison of two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, bool> greater_or_equal(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return lhs >= rhs;
			}, lhs, rhs);
		}
		/// Memberwise comparison of two matrices.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, bool> less_or_equal(
			const matrix<R, C, T> &lhs, const matrix<R, C, T> &rhs
		) {
			return operation([](const T &lhs, const T &rhs) {
				return lhs <= rhs;
			}, lhs, rhs);
		}

		/// Memberwise logical not.
		template <usize R, usize C> [[nodiscard]] constexpr static matrix<R, C, bool> logical_not(
			const matrix<R, C, bool> &m
		) {
			return operation([](bool val) {
				return !val;
			}, m);
		}
	};
	using matm = mat_memberwise; ///< Shorthand.


	// arithmetic
	/// Matrix multiplication.
	template <
		usize Rows, usize Col1Row2, usize Cols, typename U, typename V
	> [[nodiscard]] constexpr auto operator*(
		const matrix<Rows, Col1Row2, U> &lhs, const matrix<Col1Row2, Cols, V> &rhs
	) {
		using _res_type = decltype(lhs(0, 0) * rhs(0, 0) + lhs(0, 0) * rhs(0, 0));
		matrix<Rows, Cols, _res_type> result = zero;
		for (usize y = 0; y < Rows; ++y) {
			for (usize x = 0; x < Cols; ++x) {
				_res_type &res = result(y, x);
				for (usize k = 0; k < Col1Row2; ++k) {
					res += lhs(y, k) * rhs(k, x);
				}
			}
		}
		return result;
	}

	/// In-place memberwise addition.
	template <usize Rows, usize Cols, typename T, typename U> constexpr matrix<Rows, Cols, T> &operator+=(
		matrix<Rows, Cols, T> &lhs, const matrix<Rows, Cols, U> &rhs
	) {
		for (usize y = 0; y < Rows; ++y) {
			for (usize x = 0; x < Cols; ++x) {
				lhs(y, x) += rhs(y, x);
			}
		}
		return lhs;
	}
	/// Memberwise addition.
	template <
		usize Rows, usize Cols, typename T, typename U
	> [[nodiscard]] constexpr auto operator+(matrix<Rows, Cols, T> lhs, const matrix<Rows, Cols, U> &rhs) {
		return matm::operation([](const auto &vl, const auto &vr) {
			return vl + vr;
		}, lhs, rhs);
	}

	/// In-place memberwise subtraction.
	template <usize Rows, usize Cols, typename T, typename U> constexpr matrix<Rows, Cols, T> &operator-=(
		matrix<Rows, Cols, T> &lhs, const matrix<Rows, Cols, U> &rhs
	) {
		for (usize y = 0; y < Rows; ++y) {
			for (usize x = 0; x < Cols; ++x) {
				lhs(y, x) -= rhs(y, x);
			}
		}
		return lhs;
	}
	/// Memberwise subtraction.
	template <
		usize Rows, usize Cols, typename T, typename U
	> [[nodiscard]] constexpr auto operator-(matrix<Rows, Cols, T> lhs, const matrix<Rows, Cols, U> &rhs) {
		return matm::operation([](const auto &vl, const auto &vr) {
			return vl - vr;
		}, lhs, rhs);
	}
	/// Negation.
	template <
		usize Rows, usize Cols, typename T
	> [[nodiscard]] constexpr matrix<Rows, Cols, T> operator-(matrix<Rows, Cols, T> m) {
		for (usize y = 0; y < Rows; ++y) {
			for (usize x = 0; x < Cols; ++x) {
				m(y, x) = -m(y, x);
			}
		}
		return m;
	}

	/// In-place scalar multiplication.
	template <
		usize Rows, usize Cols, typename T, typename U
	> constexpr matrix<Rows, Cols, T> &operator*=(matrix<Rows, Cols, T> &lhs, const U &rhs) {
		for (usize y = 0; y < Rows; ++y) {
			for (usize x = 0; x < Cols; ++x) {
				lhs(y, x) *= rhs;
			}
		}
		return lhs;
	}
	/// Scalar multiplication.
	template <
		usize Rows, usize Cols, typename T, typename U
	> [[nodiscard]] constexpr auto operator*(const matrix<Rows, Cols, T> &lhs, const U &rhs) {
		return matm::operation([&](const T &v) {
			return v * rhs;
		}, lhs);
	}
	/// Scalar multiplication.
	template <
		usize Rows, usize Cols, typename T, typename U
	> [[nodiscard]] constexpr auto operator*(const U &lhs, const matrix<Rows, Cols, T> &rhs) {
		return matm::operation([&](const T &v) {
			return lhs * v;
		}, rhs);
	}

	/// In-place scalar division.
	template <
		usize Rows, usize Cols, typename T, typename U
	> constexpr matrix<Rows, Cols, T> &operator/=(matrix<Rows, Cols, T> &lhs, const U &rhs) {
		for (usize y = 0; y < Rows; ++y) {
			for (usize x = 0; x < Cols; ++x) {
				lhs(y, x) /= rhs;
			}
		}
		return lhs;
	}
	/// Scalar division.
	template <
		usize Rows, usize Cols, typename T, typename U
	> [[nodiscard]] constexpr auto operator/(matrix<Rows, Cols, T> lhs, const U &rhs) {
		return matm::operation([&](const auto &v) {
			return v / rhs;
		}, lhs);
	}


	template <usize N, typename T> class lup_decomposition;
	/// Matrix utilities.
	class mat {
	public:
		/// This class only contains utility functions.
		mat() = delete;
	protected:
		/// Returns the number of rows in the given matrix.
		template <typename Mat> constexpr static usize _num_rows() {
			return std::decay_t<Mat>::num_rows;
		}
		/// Returns the number of columns in the given matrix.
		template <typename Mat> constexpr static usize _num_cols() {
			return std::decay_t<Mat>::num_columns;
		}
		/// Implementation of \ref concat_columns() and \ref concat_rows().
		template <
			usize Current, typename OutMat, typename CurMat, typename ...OtherMats
		> constexpr static void _concat_impl(OutMat &out, CurMat &&cur, OtherMats &&...others) {
			constexpr bool _is_rows = _num_cols<CurMat>() == _num_cols<OutMat>();
			using _t1 = std::decay_t<OutMat>::value_type;
			static_assert(
				std::is_same_v<std::decay_t<CurMat>, zero_t> ||
				std::is_same_v<_t1, typename std::decay_t<CurMat>::value_type>,
				"mat::concat_*() does not work with matrices with different data types; "
				"use matrix::into<>() to convert data types first"
			);

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
		template <usize ...Is> [[nodiscard]] constexpr static usize _sum() {
			return (Is + ...);
		}
		/// Returns the first input.
		template <usize I, usize...> [[nodiscard]] constexpr static usize _take() {
			return I;
		}
	public:
		/// Creates a new matrix by concatenating the given matrices horizontally.
		template <typename ...Mats> [[nodiscard]] static constexpr matrix<
			_take<_num_rows<Mats>()...>(), _sum<_num_cols<Mats>()...>(), _details::first_value_type_t<Mats...>
		> concat_columns(Mats &&...mats) {
			constexpr usize
				_rows = _take<_num_rows<Mats>()...>(),
				_cols = _sum<_num_cols<Mats>()...>();
			using _t = _details::first_value_type_t<Mats...>;
			using _mat = matrix<_rows, _cols, _t>;

			static_assert(
				((_num_rows<Mats>() == _rows) && ...),
				"Arguments contain matrices with different numbers of rows"
			);
			_mat result = zero;
			_concat_impl<0>(result, std::forward<Mats>(mats)...);
			return result;
		}
		/// Creates a new matrix by concatenating the given matrices vertically.
		template <typename ...Mats> [[nodiscard]] static constexpr matrix<
			_sum<_num_rows<Mats>()...>(), _take<_num_cols<Mats>()...>(), _details::first_value_type_t<Mats...>
		> concat_rows(Mats &&...mats) {
			constexpr usize
				_rows = _sum<_num_rows<Mats>()...>(),
				_cols = _take<_num_cols<Mats>()...>();
			using _t = _details::first_value_type_t<Mats...>;
			using _mat = matrix<_rows, _cols, _t>;

			static_assert(
				((std::decay_t<Mats>::num_columns == _cols) && ...),
				"Arguments contain matrices with different numbers of columns"
			);
			_mat result = zero;
			_concat_impl<0>(result, std::forward<Mats>(mats)...);
			return result;
		}

		/// Linear interpolation.
		template <usize R, usize C, typename T> [[nodiscard]] constexpr static matrix<R, C, T> lerp(
			const matrix<R, C, T> &a, const matrix<R, C, T> &b, const T &factor
		) {
			return (1.0f - factor) * a + factor * b;
		}

		// products
		/// Kronecker product.
		template <
			usize M1, usize N1, usize M2, usize N2, typename T
		> constexpr static matrix<M1 * M2, N1 * N2, T> kronecker_product(
			const matrix<M1, N1, T> &lhs, const matrix<M2, N2, T> &rhs
		) {
			matrix<M1 * M2, N1 * N2, T> result = zero;
			usize y = 0;
			for (usize y1 = 0; y1 < M1; ++y1) {
				for (usize y2 = 0; y2 < M2; ++y2, ++y) {
					usize x = 0;
					for (usize x1 = 0; x1 < N1; ++x1) {
						for (usize x2 = 0; x2 < N2; ++x2, ++x) {
							result(y, x) = lhs(y1, x1) * rhs(y2, x2);
						}
					}
				}
			}
			return result;
		}

		/// Returns the inner product of the two matrices.
		template <typename Mat> [[nodiscard]] constexpr static Mat::value_type inner_product(
			const Mat &lhs, const Mat &rhs
		) {
			typename Mat::value_type result = zero;
			for (usize y = 0; y < Mat::num_rows; ++y) {
				for (usize x = 0; x < Mat::num_columns; ++x) {
					result += lhs(y, x) * rhs(y, x);
				}
			}
			return result;
		}

		// decomposition
		/// Shorthand for \ref lup_decomposition::compute().
		template <usize N, typename T> [[nodiscard]] constexpr static lup_decomposition<N, T> lup_decompose(
			const matrix<N, N, T>&
		);

		// hax
		/// Computes the product of the two matrices, but only the upper-right triangle; then mirrors the upper-right
		/// triangle to the bottom-left triangle. This is used for accelerating matrix products that will produce
		/// symmetric matrices.
		template <
			usize M, usize N, typename T
		> [[nodiscard]] constexpr static matrix<M, M, T> multiply_into_symmetric(
			const matrix<M, N, T> &lhs, const matrix<N, M, T> &rhs
		) {
			matrix<M, M, T> result = zero;
			for (usize y = 0; y < M; ++y) {
				for (usize x = 0; x < y; ++x) {
					result(y, x) = result(x, y);
				}
				for (usize x = y; x < M; ++x) {
					for (usize k = 0; k < N; ++k) {
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
	template <usize N, typename T> class lup_decomposition {
	public:
		/// No initialization.
		lup_decomposition(uninitialized_t) {
		}

		/// Computes the decomposition of the given matrix.
		[[nodiscard]] constexpr static lup_decomposition compute(const matrix<N, N, T> &mat) {
			lup_decomposition res(mat);
			for (usize x = 0; x < N; ++x) {
				T max = std::abs(res.result_rows(res.permutation[x], x));
				usize max_id = x;
				for (usize y = x + 1; y < N; ++y) {
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

				for (usize y = x + 1; y < N; ++y) {
					res.result_rows(res.permutation[y], x) /= res.result_rows(res.permutation[x], x);
					for (usize k = x + 1; k < N; ++k) {
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
			for (usize x = 0; x < N; ++x) {
				for (usize y = 0; y < N; ++y) {
					if (permutation[y] == x) {
						result(y, x) = static_cast<T>(1);
					}
					for (usize k = 0; k < y; ++k) {
						result(y, x) -= result_rows(permutation[y], k) * result(k, x);
					}
				}
				for (usize y = N; y > 0; ) {
					--y;
					for (usize k = y + 1; k < N; ++k) {
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
			for (usize i = 0; i < N; ++i) {
				result[i] = rhs[permutation[i]];
				for (usize k = 0; k < i; ++k) {
					result[i] -= result_rows(permutation[i], k) * result[k];
				}
			}
			for (usize i = N; i > 0; ) {
				--i;
				for (usize k = i + 1; k < N; ++k) {
					result[i] -= result_rows(permutation[i], k) * result[k];
				}
				result[i] /= result_rows(permutation[i], i);
			}
			return result;
		}

		/// Computes the determinant of the original matrix.
		[[nodiscard]] constexpr T determinant() const {
			T det = result_rows(permutation[0], 0);
			for (usize i = 1; i < N; ++i) {
				det *= result_rows(permutation[i], i);
			}
			return (num_permutations % 2 == 0) ? det : -det;
		}

		/// Rows of the decomposed matrices. The order of these rows are determined by \ref permutation. Its
		/// upper-right triangle as well as the diagonal stores the U matrix, and its lower-left triangle stores the
		/// L matrix without its diagonal. All elements on the diagonal of L are 1.
		matrix<N, N, T> result_rows = uninitialized;
		/// Permutation indices. This is the order of the rows in \ref result_rows.
		matrix<N, 1, usize> permutation = uninitialized;
		usize num_permutations; ///< Total permutation count, used for determinant computation.
	protected:
		/// Initializes this struct as the starting state of decomposition computation for the given matrix.
		/// Essentially initializes the result to the given matrix without permutation.
		constexpr explicit lup_decomposition(const matrix<N, N, T> &mat) :
			result_rows(mat), permutation(zero), num_permutations(0) {

			for (usize i = 1; i < N; ++i) {
				permutation[i] = i;
			}
		}
	};


	/// A Gauss-Seidel solver.
	class gauss_seidel {
	public:
		/// Performs one Gauss-Seidel iteration. This function modifies the result vector in-place.
		template <usize N, typename T> constexpr static void iterate(
			const matrix<N, N, T> &lhs, const matrix<N, 1, T> &rhs, matrix<N, 1, T> &result
		) {
			for (usize i = 0; i < N; ++i) {
				result[i] = rhs[i];
				for (usize j = 0; j < i; ++j) {
					result[i] -= lhs(i, j) * result[j];
				}
				for (usize j = i + 1; j < N; ++j) {
					result[i] -= lhs(i, j) * result[j];
				}
				result[i] /= lhs(i, i);
			}
		}
	};


	/// Shorthand for various matrix types.
	inline namespace matrix_types {
		template <typename T> using mat22 = matrix<2, 2, T>; ///< 2x2 matrices.
		using mat22f32 = matrix<2, 2, f32>; ///< 2x2 matrices of \ref f32.
		using mat22f64 = matrix<2, 2, f64>; ///< 2x2 matrices of \ref f64.

		template <typename T> using mat23 = matrix<2, 3, T>; ///< 2x3 matrices.
		using mat23f32 = matrix<2, 3, f32>; ///< 2x3 matrices of \ref f32.
		using mat23f64 = matrix<2, 3, f64>; ///< 2x3 matrices of \ref f64.

		template <typename T> using mat33 = matrix<3, 3, T>; ///< 3x3 matrices.
		using mat33u32 = matrix<3, 3, u32>; ///< 3x3 matrices of \ref u32.
		using mat33f32 = matrix<3, 3, f32>; ///< 3x3 matrices of \ref f32.
		using mat33f64 = matrix<3, 3, f64>; ///< 3x3 matrices of \ref f64.

		template <typename T> using mat34 = matrix<3, 4, T>; ///< 3x4 matrices.
		using mat34f32 = matrix<3, 4, f32>; ///< 3x4 matrices of \ref f32.
		using mat34f64 = matrix<3, 4, f64>; ///< 3x4 matrices of \ref f64.

		template <typename T> using mat44 = matrix<4, 4, T>; ///< 4x4 matrices.
		using mat44f32 = matrix<4, 4, f32>; ///< 4x4 matrices of \ref f32.
		using mat44f64 = matrix<4, 4, f64>; ///< 4x4 matrices of \ref f64.
	}


	// implementations
	template <
		usize Rows, usize Cols, typename T
	> constexpr matrix<Rows, Cols, T> matrix<Rows, Cols, T>::inverse() const {
		return mat::lup_decompose(*this).invert();
	}

	template <
		usize Rows, usize Cols, typename T
	> template <typename U> constexpr U matrix<Rows, Cols, T>::determinant() const requires (Rows == Cols) {
		return mat::lup_decompose(into<U>()).determinant();
	}


	template <usize N, typename T> constexpr lup_decomposition<N, T> mat::lup_decompose(
		const matrix<N, N, T> &mat
	) {
		return lup_decomposition<N, T>::compute(mat);
	}
}
