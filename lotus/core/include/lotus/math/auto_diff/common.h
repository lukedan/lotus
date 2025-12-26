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

	namespace _details {
		/// Converts a type to its corresponding \ref value_type.
		template <typename> struct to_value_type {
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
	}
}
