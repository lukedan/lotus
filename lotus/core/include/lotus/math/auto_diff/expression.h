#pragma once

/// \file
/// Expressions.

#include <string>

#include "lotus/common.h"
#include "lotus/math/numeric_traits.h"
#include "common.h"

namespace lotus::auto_diff {
	/// A type-erased expression.
	struct expression {
		friend context;
		friend _details::utils;

		/// Initializes this expression to empty.
		constexpr expression() = default;
		/// Initializes this expression to a constant.
		template <
			typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0
		> constexpr explicit expression(T v) : _constant(v) {
		}

		/// Evaluates this expression.
		template <typename T> [[nodiscard]] constexpr T eval() const;
		/// Takes the derivative of this expression with respect to the given variable.
		template <typename T> [[nodiscard]] constexpr expression diff(const variable<T>&) const;

		/// Returns the text representation of this expression.
		[[nodiscard]] constexpr std::string to_string() const;
		/// Returns the value type of this expression.
		[[nodiscard]] constexpr value_type get_value_type() const;

		/// Simplifies this expression.
		[[nodiscard]] expression simplified() const;

		/// Returns the context.
		[[nodiscard]] context &get_context() const {
			crash_if(!_ctx);
			return *_ctx;
		}
	private:
		context *_ctx = nullptr; ///< The associated context.
		union {
			const _details::operation_data *_op; ///< The underlying operation.
			f64 _constant = 0.0; ///< A constant if \ref _ctx is \p nullptr.
		};

		/// Initializes this expression with an operation.
		constexpr expression(context &ctx, const _details::operation_data &op) : _ctx(&ctx), _op(&op) {
		}
	};
}
namespace lotus {
	/// Specialization for automatically differentiated expressions.
	template <> struct numeric_traits<auto_diff::expression> {
		using value_type = auto_diff::expression; ///< Value type.

		/// Square root.
		[[nodiscard]] constexpr static value_type sqrt(value_type);
	};
}
