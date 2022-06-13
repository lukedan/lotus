#pragma once

/// \file
/// An index type where a specific value is used to indicate that the index is invalid.

#include <cstddef>
#include <limits>

#include "lotus/common.h"

namespace lotus {
	/// An index used to reference an object in an array. The type parameter is used to ensure that the index is
	/// used with a container of the correct type, and can be different from the actual value type when necessary
	/// (e.g., when the type is an incomplete type).
	template <typename Type, typename T = std::uint32_t> struct typed_index {
	public:
		/// No initialization.
		typed_index(uninitialized_t) {
		}
		/// Initializes this index.
		typed_index(T v) : _value(v) {
		}

		/// Default equality and inequality.
		friend bool operator==(const typed_index&, const typed_index&) = default;

		/// Returns the value of this index.
		[[nodiscard]] T get_value() const {
			return _value;
		}
	private:
		T _value; ///< The index value.
	};
	/// An index type where a specific value is used to indicate that the index is invalid.
	template <
		typename Type, typename T = std::uint32_t, T InvalidValue = std::numeric_limits<T>::max()
	> struct optional_typed_index {
	public:
		using non_optional_type = typed_index<Type, T>; ///< Non-optional version of this index.

		/// Initializes this index to be invalid.
		optional_typed_index(std::nullptr_t) : _value(InvalidValue) {
		}
		/// Initializes this index with a valid value.
		optional_typed_index(non_optional_type v) : _value(v) {
			assert(is_valid());
		}

		/// Default equality and inequality.
		friend bool operator==(const optional_typed_index&, const optional_typed_index&) = default;

		/// Returns the value of this index.
		[[nodiscard]] non_optional_type get() const {
			assert(is_valid());
			return _value;
		}

		/// Returns whether this index is valid.
		[[nodiscard]] bool is_valid() const {
			return _value.get_value() != InvalidValue;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		non_optional_type _value; ///< The index value.
	};
}
