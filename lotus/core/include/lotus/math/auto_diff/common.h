#pragma once

/// \file
/// Common types for auto differentiation.

#include <type_traits>

#include "lotus/types.h"

namespace lotus::auto_diff {
	class context;
	struct expression;
	template <typename> struct variable;
	namespace _details {
		struct variable_data;
		struct operation_data;
		struct utils;
	}

	/// Value type.
	enum class value_type {
		f32, ///< 32-bit floating point.
		f64, ///< 64-bit floating point.
	};
	/// A type-erased expression.
	struct expression {
		friend context;
		friend _details::utils;

		/// Initializes this expression to empty.
		constexpr expression() = default;

		/// Evaluates this expression.
		template <typename T> [[nodiscard]] constexpr T eval() const;
		/// Takes the derivative of this expression with respect to the given variable.
		template <typename T> [[nodiscard]] constexpr expression diff(const variable<T>&) const;

		/// Returns the text representation of this expression.
		[[nodiscard]] constexpr std::string to_string() const;
		/// Returns the value type of this expression.
		[[nodiscard]] constexpr value_type get_value_type() const;

		/// Returns the context.
		[[nodiscard]] context &get_context() const {
			return *_ctx;
		}
	private:
		context *_ctx = nullptr; ///< The associated context.
		_details::operation_data *_op = nullptr; ///< The underlying operation.

		/// Initializes this expression with an operation.
		constexpr expression(context &ctx, _details::operation_data &op) : _ctx(&ctx), _op(&op) {
		}
	};
	/// A variable that can be automatically differentiated with respect to.
	template <typename T> struct variable {
		friend context;
		friend expression;
		friend _details::utils;

		using value_type = T; ///< Value type.

		/// Does not bind this variable.
		constexpr variable() = default;

		/// Returns the value of this variable.
		[[nodiscard]] constexpr value_type get_value() const;

		/// Returns the context.
		[[nodiscard]] context &get_context() const {
			return *_ctx;
		}

		/// Converts this variable into an expression.
		[[nodiscard]] expression into_expression() const;

		/// Returns whether this object is bound to an actual variable.
		[[nodiscard]] constexpr bool is_valid() const {
			return _data;
		}
		/// Shorthand for \ref is_valid().
		[[nodiscard]] constexpr explicit operator bool() const {
			return is_valid();
		}
	private:
		context *_ctx = nullptr; ///< The associated context.
		_details::variable_data *_data = nullptr; ///< The data associated with this variable.

		/// Binds this variable to the given \ref _details::variable_data object.
		constexpr variable(context &ctx, _details::variable_data &data) : _ctx(&ctx), _data(&data) {
		}
	};

	namespace _details {
		/// Struct used to test if a type is a specialization of \ref variable.
		template <typename T> struct is_variable {
			constexpr static bool value = false; ///< Not a variable.
		};
		/// Variables are variables.
		template <typename T> struct is_variable<variable<T>> {
			constexpr static bool value = true; ///< Variable.
		};
		/// Shorthand for \ref is_variable::value.
		template <typename T> constexpr bool is_variable_v = is_variable<T>::value;

		/// Converts a type to its corresponding \ref value_type.
		template <typename T> struct to_value_type {
		};
		/// Conversion for 32-bit floating point.
		template <> struct to_value_type<f32> {
			constexpr static value_type value = value_type::f32; ///< 32-bit floating point.
		};
		/// Conversion for 64-bit floating point.
		template <> struct to_value_type<f64> {
			constexpr static value_type value = value_type::f64; ///< 64-bit floating point.
		};
		/// Shorthand for \ref to_value_type::value.
		template <typename T> constexpr value_type to_value_type_v = to_value_type<T>::value;

		/// Internal data associated with a variable.
		struct variable_data {
			/// Initializes \ref type.
			constexpr variable_data(std::string n, value_type t, f64 value) : name(std::move(n)), type(t) {
				set_value(value);
			}

			union {
				f32 value_f32; ///< Value as 32-bit float.
				f64 value_f64; ///< Value as 64-bit float.
			};
			std::string name; ///< The name of this variable.
			const value_type type; ///< The type of this variable.

			/// Sets the value of this variable.
			constexpr void set_value(f64 v) {
				switch (type) {
				case value_type::f32:
					value_f32 = static_cast<f32>(v);
					break;
				case value_type::f64:
					value_f64 = v;
					break;
				default:
					std::abort();
				}
			}
			/// Returns the value of this variable, cast to the given type.
			template <typename T> [[nodiscard]] constexpr T get_value() const {
				switch (type) {
				case value_type::f32:
					return static_cast<T>(value_f32);
				case value_type::f64:
					return static_cast<T>(value_f64);
				}
				std::abort();
			}
		};
	}


	template <typename T> constexpr T variable<T>::get_value() const {
		return _data->get_value<value_type>();
	}
}
