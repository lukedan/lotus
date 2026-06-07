#pragma once

/// \file
/// Utilities for auto differentiation.

#include "lotus/math/matrix.h"
#include "lotus/math/vector.h"

#include "lotus/math/auto_diff/context.h"

namespace lotus::auto_diff {
	/// Matrix related utility functions.
	class mat {
	public:
		/// This class only contains utility functions.
		mat() = delete;

		/// Converts a matrix of variables into a matrix of expressions.
		template <
			usize Rows, usize Cols, typename T
		> [[nodiscard]] constexpr static matrix<Rows, Cols, expression> into_expression(
			const matrix<Rows, Cols, variable<T>> &m
		) {
			return matm::operation(
				[](const variable<T> &v) {
					return v.into_expression();
				},
				m
			);
		}

		/// Evaluates a matrix.
		template <typename T, usize Rows, usize Cols> constexpr static matrix<Rows, Cols, T> eval(
			const matrix<Rows, Cols, expression> &m
		) {
			return matm::operation(
				[](const expression &e) {
					return e.eval<T>();
				},
				m
			);
		}
		/// Takes the derivative of the matrix against the given variable.
		template <usize Rows, usize Cols, typename T> constexpr static matrix<Rows, Cols, expression> diff(
			const matrix<Rows, Cols, expression> &m, const variable<T> &v
		) {
			return matm::operation(
				[&](const expression &e) {
					return e.diff(v);
				},
				m
			);
		}
		/// Takes the derivative of the scalar against the vector, in denominator layout.
		template <usize Rows, typename T> constexpr static column_vector<Rows, expression> diff_denominator(
			const expression &y, const column_vector<Rows, variable<T>> &x
		) {
			return matm::operation(
				[&](const variable<T> &v) {
					return y.diff(v);
				},
				x
			);
		}
		/// Takes the derivative of the vector against another vector, in denominator layout.
		template <
			usize YRows, usize XRows, typename T
		> constexpr static matrix<XRows, YRows, expression> diff_denominator(
			const column_vector<YRows, expression> &y, const column_vector<XRows, variable<T>> &x
		) {
			matrix<XRows, YRows, expression> result = zero;
			for (usize r = 0; r < XRows; ++r) {
				for (usize c = 0; c < YRows; ++c) {
					result(r, c) = y[c].diff(x[r]);
				}
			}
			return result;
		}

		/// Simplifies all elements of the matrix.
		template <usize Rows, usize Cols> constexpr static matrix<Rows, Cols, expression> simplify(
			const matrix<Rows, Cols, expression> &m
		) {
			return matm::operation(
				[&](const expression &e) {
					return e.simplified();
				},
				m
			);
		}
	};
}
