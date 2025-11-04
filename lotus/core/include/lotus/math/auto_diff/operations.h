#pragma once

/// \file
/// Operations on automatically differentiated scalars.

#include "lotus/common.h"

#include "common.h"

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
			return std::to_string(value);
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
	struct unary_operands {
		_details::operation_data *op; ///< The operand.
	};

	/// Square root.
	template <typename Scalar> struct sqrt {
		using value_type = Scalar; ///< Value type.

		unary_operands ops; ///< Operands.

		/// Takes the square root of the operand.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the deriative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "sqrt(op)".
		[[nodiscard]] constexpr std::string to_string() const;
	};

	/// Operands of a binary operation.
	struct binary_operands {
		_details::operation_data *lhs; ///< The left hand side operand.
		_details::operation_data *rhs; ///< The right hand side operand.
	};

	/// Addition.
	template <typename Scalar> struct add {
		using value_type = Scalar; ///< Value type.

		binary_operands ops; ///< Operands.

		/// Adds the two operands.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the derivative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "lhs + rhs".
		[[nodiscard]] constexpr std::string to_string() const;
	};
	/// Subtraction.
	template <typename Scalar> struct subtract {
		using value_type = Scalar; ///< Value type.

		binary_operands ops; ///< Operands.

		/// Subtracts the two operands.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the derivative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "lhs - rhs".
		[[nodiscard]] constexpr std::string to_string() const;
	};
	/// Multiplication.
	template <typename Scalar> struct multiply {
		using value_type = Scalar; ///< Value type.

		binary_operands ops; ///< Operands.

		/// Multiplies the two operands.
		[[nodiscard]] constexpr value_type eval() const;
		/// Takes the derivative of this operation with respect to the given variable.
		[[nodiscard]] constexpr expression diff(const _details::variable_data&, context&) const;

		/// Returns "lhs * rhs".
		[[nodiscard]] constexpr std::string to_string() const;
	};
	/// Division.
	template <typename Scalar> struct divide {
		using value_type = Scalar; ///< Value type.

		binary_operands ops; ///< Operands.

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

	/// Returns the \ref value_type of an operand.
	template <typename T> [[nodiscard]] constexpr value_type get_operand_value_type(const T &op) {
		if constexpr (std::is_arithmetic_v<T>) {
			return to_value_type_v<T>;
		} else if constexpr (std::is_same_v<T, expression>) {
			return op.get_value_type();
		} else {
			return to_value_type_v<typename T::value_type>;
		}
	}
}


namespace lotus::auto_diff {
	template <typename T> constexpr T expression::eval() const {
		return _op->eval<T>();
	}

	template <typename T> constexpr expression expression::diff(const variable<T> &v) const {
		return _op->diff(*v._data, *_ctx);
	}

	constexpr std::string expression::to_string() const {
		return _op->to_string();
	}

	constexpr value_type expression::get_value_type() const {
		return _op->get_value_type();
	}


	namespace operations {
		template <typename T> constexpr T sqrt<T>::eval() const {
			return std::sqrt(ops.op->eval<value_type>());
		}

		template <typename T> constexpr std::string sqrt<T>::to_string() const {
			return "sqrt(" + ops.op->to_string() + ")";
		}

		template <typename T> constexpr T add<T>::eval() const {
			return ops.lhs->eval<value_type>() + ops.rhs->eval<value_type>();
		}

		template <typename T> constexpr std::string add<T>::to_string() const {
			return "(" + ops.lhs->to_string() + " + " + ops.rhs->to_string() + ")";
		}

		template <typename T> constexpr T subtract<T>::eval() const {
			return ops.lhs->eval<value_type>() - ops.rhs->eval<value_type>();
		}

		template <typename T> constexpr std::string subtract<T>::to_string() const {
			return "(" + ops.lhs->to_string() + " - " + ops.rhs->to_string() + ")";
		}

		template <typename T> constexpr T multiply<T>::eval() const {
			return ops.lhs->eval<value_type>() * ops.rhs->eval<value_type>();
		}

		template <typename T> constexpr std::string multiply<T>::to_string() const {
			return "(" + ops.lhs->to_string() + " * " + ops.rhs->to_string() + ")";
		}

		template <typename T> constexpr T divide<T>::eval() const {
			return ops.lhs->eval<value_type>() / ops.rhs->eval<value_type>();
		}

		template <typename T> constexpr std::string divide<T>::to_string() const {
			return "(" + ops.lhs->to_string() + " / " + ops.rhs->to_string() + ")";
		}
	}
}
