#pragma once

/// \file
/// Operations on automatically differentiated scalars.

#include <variant>
#include <format>

#include "lotus/common.h"

#include "common.h"
#include "variable.h"
#include "expression.h"

namespace lotus::auto_diff::operations {
	/// Constant.
	template <typename T> struct constant {
		using value_type = T; ///< Value type.

		value_type value; ///< The value of this constant.

		/// Evaluates this constant.
		[[nodiscard]] constexpr value_type eval() const {
			return value;
		}
		/// Returns zero.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Converts \ref value into a string.
		[[nodiscard]] constexpr std::string to_string() const {
			return std::format("{}", value);
		}
	};
	/// Variable.
	template <typename T> struct variable {
		using value_type = T; ///< Value type.

		_details::variable_data *var; ///< The associated variable.

		/// Evaluates the variable.
		[[nodiscard]] constexpr value_type eval() const {
			return var->get_value<value_type>();
		}
		/// Returns either zero or one depending on whether we're differentiating against the same variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns the name of the variable.
		[[nodiscard]] constexpr std::string to_string() const {
			return var->name;
		}
	};

	/// Operands of a unary operation.
	struct unary_operation {
		const _details::operation_data *op; ///< The operand.
	};

	/// Square root.
	template <typename Scalar> struct sqrt : unary_operation {
		using value_type = Scalar; ///< Value type.

		/// Initializes the operands.
		explicit sqrt(const _details::operation_data *op) : unary_operation(op) {
		}

		/// Takes the square root of the operand.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the deriative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "sqrt(op)".
		[[nodiscard]] constexpr std::string to_string() const;
	};

	/// Operands of a binary operation.
	struct binary_operation {
		const _details::operation_data *lhs; ///< The left hand side operand.
		const _details::operation_data *rhs; ///< The right hand side operand.
	};

	/// Addition.
	template <typename Scalar> struct add : binary_operation {
		using value_type = Scalar; ///< Value type.

		/// Initializes the operands.
		add(const _details::operation_data *lhs, const _details::operation_data *rhs) : binary_operation(lhs, rhs) {
		}

		/// Adds the two operands.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the derivative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "lhs + rhs".
		[[nodiscard]] constexpr std::string to_string() const;
	};
	/// Subtraction.
	template <typename Scalar> struct subtract : binary_operation {
		using value_type = Scalar; ///< Value type.

		/// Initializes the operands.
		subtract(const _details::operation_data *lhs, const _details::operation_data *rhs) :
			binary_operation(lhs, rhs) {
		}

		/// Subtracts the two operands.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the derivative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "lhs - rhs".
		[[nodiscard]] constexpr std::string to_string() const;
	};
	/// Multiplication.
	template <typename Scalar> struct multiply : binary_operation {
		using value_type = Scalar; ///< Value type.

		/// Initializes the operands.
		multiply(const _details::operation_data *lhs, const _details::operation_data *rhs) :
			binary_operation(lhs, rhs) {
		}

		/// Multiplies the two operands.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the derivative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "lhs * rhs".
		[[nodiscard]] constexpr std::string to_string() const;
	};
	/// Division.
	template <typename Scalar> struct divide : binary_operation {
		using value_type = Scalar; ///< Value type.

		/// Initializes the operands.
		divide(const _details::operation_data *lhs, const _details::operation_data *rhs) :
			binary_operation(lhs, rhs) {
		}

		/// Divides the first operand by the second one.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the derivative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "lhs / rhs".
		[[nodiscard]] constexpr std::string to_string() const;
	};
}

namespace lotus::auto_diff::_details {
	/// Internal representation of an operation.
	struct operation_data {
		std::variant<
			operations::constant<f32>,
			operations::constant<f64>,
			operations::variable<f32>,
			operations::variable<f64>,

			operations::sqrt<f32>,
			operations::sqrt<f64>,

			operations::add<f32>,
			operations::add<f64>,
			operations::subtract<f32>,
			operations::subtract<f64>,
			operations::multiply<f32>,
			operations::multiply<f64>,
			operations::divide<f32>,
			operations::divide<f64>
		> operation; ///< The operation.

		/// Evaluates this operation.
		template <typename T> [[nodiscard]] constexpr T eval() const {
			return std::visit(
				[](const auto &op) {
					return static_cast<T>(op.eval());
				},
				operation
			);
		}
		/// Takes the derivative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const variable_data &v, context &ctx) const {
			return std::visit(
				[&](const auto &op) {
					return op.diff(v, ctx);
				},
				operation
			);
		}

		/// Converts this operation to string.
		[[nodiscard]] constexpr std::string to_string() const {
			return std::visit(
				[](const auto &op) {
					return op.to_string();
				},
				operation
			);
		}
		/// Returns the value type returned by this operation.
		[[nodiscard]] constexpr value_type get_value_type() const {
			return std::visit(
				[]<typename Op>(const Op&) {
					return to_value_type_v<typename Op::value_type>;
				},
				operation
			);
		}
	};

	/// Used to determine if a type is an expression or a variable.
	template <typename> struct is_operand {
		constexpr static bool value = false; ///< Not expressions by default.
	};
	/// Expressions are operands.
	template <> struct is_operand<expression> {
		constexpr static bool value = true; ///< Value.
	};
	/// Variables are operands.
	template <typename T> struct is_operand<variable<T>> {
		constexpr static bool value = true; ///< Value.
	};
	/// Shorthand for \ref is_operand::value.
	template <typename T> constexpr bool is_operand_v = is_operand<T>::value;
}


namespace lotus::auto_diff {
	template <typename T> constexpr T expression::eval() const {
		if (!_ctx) {
			return static_cast<T>(_constant);
		}
		return _op->eval<T>();
	}

	template <typename T> constexpr expression expression::diff(const variable<T> &v) const {
		if (!_ctx) {
			return expression(0.0f);
		}
		return _op->diff(*v._data, *_ctx);
	}

	constexpr std::string expression::to_string() const {
		if (!_ctx) {
			return std::format("{}", _constant);
		}
		return _op->to_string();
	}

	constexpr value_type expression::get_value_type() const {
		return _op->get_value_type();
	}


	namespace operations {
		template <typename T> constexpr T sqrt<T>::eval() const {
			return std::sqrt(op->eval<value_type>());
		}

		template <typename T> constexpr std::string sqrt<T>::to_string() const {
			return "sqrt(" + op->to_string() + ")";
		}

		template <typename T> constexpr T add<T>::eval() const {
			return lhs->eval<value_type>() + rhs->eval<value_type>();
		}

		template <typename T> constexpr std::string add<T>::to_string() const {
			return "(" + lhs->to_string() + " + " + rhs->to_string() + ")";
		}

		template <typename T> constexpr T subtract<T>::eval() const {
			return lhs->eval<value_type>() - rhs->eval<value_type>();
		}

		template <typename T> constexpr std::string subtract<T>::to_string() const {
			return "(" + lhs->to_string() + " - " + rhs->to_string() + ")";
		}

		template <typename T> constexpr T multiply<T>::eval() const {
			return lhs->eval<value_type>() * rhs->eval<value_type>();
		}

		template <typename T> constexpr std::string multiply<T>::to_string() const {
			return "(" + lhs->to_string() + " * " + rhs->to_string() + ")";
		}

		template <typename T> constexpr T divide<T>::eval() const {
			return lhs->eval<value_type>() / rhs->eval<value_type>();
		}

		template <typename T> constexpr std::string divide<T>::to_string() const {
			return "(" + lhs->to_string() + " / " + rhs->to_string() + ")";
		}
	}
}
