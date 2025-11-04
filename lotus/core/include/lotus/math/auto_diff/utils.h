#pragma once

/// \file
/// Utilities for auto differentiation.

#include "lotus/math/matrix.h"
#include "context.h"

namespace lotus::auto_diff {
	/// Matrix related utility functions.
	class mat {
	public:
		/// This class only contains utility functions.
		mat() = delete;

		/// Converts a matrix of variables into a matrix of expressions.
		template <
			usize Rows, usize Cols, typename T
		> constexpr static matrix<Rows, Cols, expression> into_expression(const matrix<Rows, Cols, variable<T>> &m) {
			return matm::operation(
				[](const variable<T> &v) {
					return v.into_expression();
				},
				m
			);
		}
	};
}
