#pragma once

/// \file
/// Auto differentiation contexts.

#include <deque>

#include "common.h"
#include "operations.h"

namespace lotus::auto_diff {
	/// Stores information about all variables and acts as an allocator for all dynamic expressions.
	class context {
		friend _details::utils;
	public:
		/// Initializes this context.
		context() :
			_zero_op(operations::constant<f32>(0.0f)),
			_one_op(operations::constant<f32>(1.0f)),
			_zero_exp(*this, _zero_op),
			_one_exp(*this, _one_op) {
		}
		/// No copy construction.
		context(const context&) = delete;
		/// No copy assignment.
		context &operator=(const context&) = delete;

		/// Creates a new variable.
		template <typename Scalar> [[nodiscard]] constexpr variable<Scalar> create_variable(std::string name, Scalar value) {
			return variable<Scalar>(*this, _variables.emplace_back(
				std::move(name), _details::to_value_type_v<Scalar>, value
			));
		}
		/// Creates a matrix of new variables.
		template <
			usize Rows, usize Cols, typename Scalar
		> [[nodiscard]] constexpr matrix<Rows, Cols, variable<Scalar>> create_matrix_variable(
			const std::string &name, matrix<Rows, Cols, Scalar> value
		) {
			matrix<Rows, Cols, variable<Scalar>> result = zero;
			for (usize r = 0; r < Rows; ++r) {
				for (usize c = 0; c < Cols; ++c) {
					result(r, c) = create_variable<Scalar>(
						name + std::to_string(r + 1) + std::to_string(c + 1), value(r, c)
					);
				}
			}
			return result;
		}

		/// Returns an expression representing zero.
		[[nodiscard]] constexpr expression get_zero_expression() const {
			return _zero_exp;
		}
		/// Returns an expression representing one.
		[[nodiscard]] constexpr expression get_one_expression() const {
			return _one_exp;
		}
	private:
		_details::operation_data _zero_op; ///< An operation that represents constant zero.
		_details::operation_data _one_op; ///< An operation that represents constant one.
		expression _zero_exp; ///< Expression representing \ref _zero_op.
		expression _one_exp; ///< Expression representing \ref _one_op.

		std::deque<_details::variable_data> _variables; ///< All registered variables.
		std::deque<_details::operation_data> _operations; ///< All registered operations.
	};
}

namespace lotus::auto_diff::_details {
	/// Gathers metadata from a 2-operand operation.
	template <typename Lhs, typename Rhs> [[nodiscard]] constexpr std::pair<context&, value_type> get_metadata(
		const Lhs &lhs, const Rhs &rhs
	) {
		context *ctx = nullptr;
		if constexpr (_details::is_operand_v<Lhs>) {
			ctx = &lhs.get_context();
		} else {
			ctx = &rhs.get_context();
		}
		const value_type res_ty = std::max(
			_details::get_operand_value_type(lhs), _details::get_operand_value_type(rhs)
		);
		return { *ctx, res_ty };
	}

	/// Utilities that can access private members.
	struct utils {
		/// Wraps an operation in an expression.
		[[nodiscard]] constexpr static expression make_expr(context &ctx, operation_data &op) {
			return expression(ctx, op);
		}

		/// Creates a new expression.
		template <typename T, typename ...Args> [[nodiscard]] constexpr static operation_data &new_op(
			context &ctx, Args &&...args
		) {
			return ctx._operations.emplace_back(T(std::forward<Args>(args)...));
		}

		/// Type erases an operand.
		template <typename T> [[nodiscard]] constexpr static operation_data &to_operand(
			const T &op, context &ctx
		) {
			if constexpr (std::is_same_v<T, f32>) {
				return new_op<operations::constant<f32>>(ctx, op);
			} else if constexpr (std::is_same_v<T, f64>) {
				return new_op<operations::constant<f64>>(ctx, op);
			} else if constexpr (is_variable_v<T>) {
				return new_op<operations::variable<typename T::value_type>>(ctx, op._data);
			} else if constexpr (std::is_same_v<T, expression>) {
				return *op._op;
			} else {
				static_assert(false, "Invalid type for operand");
			}
		}
	};

	/// Default implementation of a binary operator that only fills in the operands in a specific order. Assumes that
	/// the operation has one template argument indicating the value type.
	template <
		template <typename> typename Op, typename Lhs, typename Rhs
	> [[nodiscard]] constexpr expression default_binary_operator_impl(const Lhs &lhs, const Rhs &rhs) {
		const auto [ctx, res_ty] = get_metadata(lhs, rhs);
		operation_data *lhs_op = &utils::to_operand(lhs, ctx);
		operation_data *rhs_op = &utils::to_operand(rhs, ctx);

		switch (res_ty) {
		case value_type::f32:
			return utils::make_expr(ctx, utils::new_op<Op<f32>>(ctx, operations::binary_operands(lhs_op, rhs_op)));
		case value_type::f64:
			return utils::make_expr(ctx, utils::new_op<Op<f64>>(ctx, operations::binary_operands(lhs_op, rhs_op)));
		default:
			std::abort();
		}
	}
}

namespace lotus::auto_diff::inline operators {
	/// Addition operator.
	template <typename Lhs, typename Rhs> [[nodiscard]] constexpr std::enable_if_t<
		_details::is_operand_v<Lhs> || _details::is_operand_v<Rhs>, expression
	> operator+(const Lhs &lhs, const Rhs &rhs) {
		return _details::default_binary_operator_impl<operations::add>(lhs, rhs);
	}
	/// In-place addition operator.
	template <typename Rhs> constexpr expression &operator+=(expression &lhs, const Rhs &rhs) {
		return lhs = lhs + rhs;
	}

	/// Subtraction operator.
	template <typename Lhs, typename Rhs> [[nodiscard]] constexpr std::enable_if_t<
		_details::is_operand_v<Lhs> || _details::is_operand_v<Rhs>, expression
	> operator-(const Lhs &lhs, const Rhs &rhs) {
		return _details::default_binary_operator_impl<operations::subtract>(lhs, rhs);
	}
	/// In-place subtraction operator.
	template <typename Rhs> constexpr expression &operator-=(expression &lhs, const Rhs &rhs) {
		return lhs = lhs - rhs;
	}

	/// Multiplication operator.
	template <typename Lhs, typename Rhs> [[nodiscard]] constexpr std::enable_if_t<
		_details::is_operand_v<Lhs> || _details::is_operand_v<Rhs>, expression
	> operator*(const Lhs &lhs, const Rhs &rhs) {
		return _details::default_binary_operator_impl<operations::multiply>(lhs, rhs);
	}
	/// In-place multiplication operator.
	template <typename Rhs> constexpr expression &operator*=(expression &lhs, const Rhs &rhs) {
		return lhs = lhs * rhs;
	}

	/// Division operator.
	template <typename Lhs, typename Rhs> [[nodiscard]] constexpr std::enable_if_t<
		_details::is_operand_v<Lhs> || _details::is_operand_v<Rhs>, expression
	> operator/(const Lhs &lhs, const Rhs &rhs) {
		return _details::default_binary_operator_impl<operations::divide>(lhs, rhs);
	}
	/// In-place division operator.
	template <typename Rhs> constexpr expression &operator/=(expression &lhs, const Rhs &rhs) {
		return lhs = lhs / rhs;
	}
}


namespace lotus::auto_diff {
	template <typename T> expression variable<T>::into_expression() const {
		return _details::utils::make_expr(*_ctx, _details::utils::new_op<operations::variable<T>>(*_ctx, _data));
	}


	namespace operations {
		template <typename T> constexpr expression constant<T>::diff(
			const _details::variable_data&, context &ctx
		) const {
			return ctx.get_zero_expression();
		}

		template <typename T> constexpr expression variable<T>::diff(
			const _details::variable_data &v, context &ctx
		) const {
			return &v == var ? ctx.get_one_expression() : ctx.get_zero_expression();
		}

		template <typename T> constexpr expression add<T>::diff(
			const _details::variable_data &v, context &ctx
		) const {
			return ops.lhs->diff(v, ctx) + ops.rhs->diff(v, ctx);
		}

		template <typename T> constexpr expression subtract<T>::diff(
			const _details::variable_data &v, context &ctx
		) const {
			return ops.lhs->diff(v, ctx) - ops.rhs->diff(v, ctx);
		}

		template <typename T> constexpr expression multiply<T>::diff(
			const _details::variable_data &v, context &ctx
		) const {
			const expression lhs = _details::utils::make_expr(ctx, *ops.lhs);
			const expression rhs = _details::utils::make_expr(ctx, *ops.rhs);
			return ops.lhs->diff(v, ctx) * rhs + lhs * ops.rhs->diff(v, ctx);
		}

		template <typename T> constexpr expression divide<T>::diff(
			const _details::variable_data &v, context &ctx
		) const {
			const expression lhs = _details::utils::make_expr(ctx, *ops.lhs);
			const expression rhs = _details::utils::make_expr(ctx, *ops.rhs);
			return (ops.lhs->diff(v, ctx) * rhs - lhs * ops.rhs->diff(v, ctx)) / (rhs * rhs);
		}
	}
}
