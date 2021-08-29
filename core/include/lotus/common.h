#pragma once

/// \file
/// Common types and functions.

#include <type_traits>

namespace lotus {
	/// A type indicating a specific object should not be initialized.
	struct uninitialized_t {
		/// Implicit conversion to arithmetic types and pointers.
		template <
			typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_pointer_v<T>, int> = 0
		> operator T() const {
			return T{};
		}
	};
	/// A type indicating a specific object should be zero-initialized.
	struct zero_t {
		/// Implicit conversion to arithmetic types and pointers.
		template <
			typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_pointer_v<T>, int> = 0
		> constexpr operator T() const {
			return static_cast<T>(0);
		}
	};
	constexpr inline uninitialized_t uninitialized; ///< An instance of \ref uninitialized_t.
	constexpr inline zero_t zero; ///< An instance of \ref zero_t.


	/// Indicates if all following bitwise operators are enabled for a type. Enum classes can create specializations
	/// to enable them.
	template <typename> struct enable_enum_bitwise_operators : public std::false_type {
	};
	/// Shorthand for \ref enable_enum_bitwise_operators.
	template <typename T> constexpr static bool enable_enum_bitwise_operators_v =
		enable_enum_bitwise_operators<T>::value;

	/// Bitwise and for enum classes.
	template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator&(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) & static_cast<_base>(rhs));
	}
	/// Bitwise or for enum classes.
	template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator|(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) | static_cast<_base>(rhs));
	}
	/// Bitwise xor for enum classes.
	template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator^(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) ^ static_cast<_base>(rhs));
	}
	/// Bitwise not for enum classes.
	template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator~(Enum v) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(~static_cast<_base>(v));
	}

	/// Bitwise and for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator&=(Enum &lhs, Enum rhs) {
		return lhs = lhs & rhs;
	}
	/// Bitwise or for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator|=(Enum &lhs, Enum rhs) {
		return lhs = lhs | rhs;
	}
	/// Bitwise xor for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator^=(Enum &lhs, Enum rhs) {
		return lhs = lhs ^ rhs;
	}


	/// Indicates if an enum type can be used with \ref is_empty().
	template <typename> struct enable_enum_is_empty : public std::false_type {
	};
	/// Shorthand for \ref enable_enum_is_empty.
	template <typename T> constexpr static bool enable_enum_is_empty_v =
		enable_enum_is_empty<T>::value;

	/// Tests if an enum is zero.
	template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_is_empty_v<Enum>, bool
	> is_empty(Enum v) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<_base>(v) != _base(0);
	}
}
