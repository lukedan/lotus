#include "lotus/math/auto_diff/expression.h"

/// \file
/// Implementation of automatically differentiated expressions.

#include "lotus/math/auto_diff/utils.h"

namespace lotus::auto_diff {
	/// Checks if the given type is a specialization of another type.
	template <typename, template <typename> typename> struct _is_specialization_of {
		constexpr static bool value = false; ///< The value.
	};
	/// Specialization for specializations.
	template <template <typename> typename T, typename U> struct _is_specialization_of<T<U>, T> {
		constexpr static bool value = true; ///< The value.
	};
	/// Shorthand for \ref _is_specialization_of::value.
	template <typename T, template <typename> typename U> constexpr static bool _is_specialization_of_v =
		_is_specialization_of<T, U>::value;

	/// Checks if the given operation is constant zero.
	[[nodiscard]] static bool _is_constant_zero(const _details::operation_data &op_data) {
		return std::visit(
			[]<template <typename> typename Op, typename T>(const Op<T> &op) {
				if constexpr (std::is_same_v<Op<T>, operations::constant<T>>) {
					return op.value == 0.0f;
				}
				return false;
			},
			op_data.operation
		);
	}
	/// Simplifies a binary operation.
	template <typename Op> [[nodiscard]] static const _details::operation_data *_simplify_op(
		const _details::operation_data *lhs, const _details::operation_data *rhs, context &ctx
	) {
		if constexpr (_is_specialization_of_v<Op, operations::add>) {
			if (_is_constant_zero(*lhs)) {
				return rhs;
			}
			if (_is_constant_zero(*rhs)) {
				return lhs;
			}
		} else if constexpr (_is_specialization_of_v<Op, operations::subtract>) {
			if (_is_constant_zero(*rhs)) {
				return lhs;
			}
			if (lhs == rhs) {
				return _details::utils::get_op(ctx.get_zero_expression());
			}
		} else if constexpr (_is_specialization_of_v<Op, operations::multiply>) {
			if (_is_constant_zero(*lhs) || _is_constant_zero(*rhs)) {
				return _details::utils::get_op(ctx.get_zero_expression());
			}
		} else if constexpr (_is_specialization_of_v<Op, operations::divide>) {
			if (_is_constant_zero(*lhs)) {
				return _details::utils::get_op(ctx.get_zero_expression());
			}
		}
		return nullptr;
	}
	/// Simplifies the given expression.
	[[nodiscard]] static const _details::operation_data *_simplify_expression(
		const _details::operation_data *op_data, context &ctx
	) {
		const _details::operation_data *result = op_data;
		std::visit(
			[&]<typename Op>(const Op &op) {
				if constexpr (std::is_base_of_v<operations::unary_operation, Op>) {
					auto &unary_op = static_cast<const operations::unary_operation&>(op);
					const _details::operation_data *simplified = _simplify_expression(unary_op.op, ctx);
					if (simplified != op_data) {
						result = &_details::utils::new_op<Op>(ctx, simplified);
					}
				} else if constexpr (std::is_base_of_v<operations::binary_operation, Op>) {
					auto &binary_op = static_cast<const operations::binary_operation&>(op);
					const _details::operation_data *lhs_simp = _simplify_expression(binary_op.lhs, ctx);
					const _details::operation_data *rhs_simp = _simplify_expression(binary_op.rhs, ctx);

					// optimizations
					if (const _details::operation_data *optimized = _simplify_op<Op>(lhs_simp, rhs_simp, ctx)) {
						result = optimized;
					} else {
						if (lhs_simp != binary_op.lhs || rhs_simp != binary_op.rhs) {
							result = &_details::utils::new_op<Op>(ctx, lhs_simp, rhs_simp);
						}
					}
				} else {
					// nothing to do - return as is
				}
			},
			op_data->operation
		);
		return result;
	}
	expression expression::simplified() const {
		if (!_ctx) {
			return *this;
		}
		return _details::utils::make_expr(*_ctx, *_simplify_expression(_op, *_ctx));
	}
}
