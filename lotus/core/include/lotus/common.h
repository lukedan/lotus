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
		return static_cast<_base>(v) == _base(0);
	}


	/// Stores a mapping from consecutive zero-based enum values to mapped values, with additional checks.
	template <
		typename Enum, typename Value, std::size_t NumEnumerators = static_cast<std::size_t>(Enum::num_enumerators)
	> class enum_mapping {
	public:
		/// Initializes the mapping.
		template <typename ...Args> constexpr enum_mapping(Args &&...args) :
			_mapping{ { std::forward<Args>(args)... } } {
			static_assert(sizeof...(args) == NumEnumerators, "Incorrect number of entries for enum mapping.");
			for (std::size_t i = 0; i < NumEnumerators; ++i) {
				assert(static_cast<std::size_t>(_mapping[i].first) == i);
			}
		}

		/// Retrieves the mapping for the given value.
		[[nodiscard]] constexpr const Value &operator[](Enum v) const {
			return _mapping[static_cast<std::size_t>(v)].second;
		}
		/// Returns the entire table.
		[[nodiscard]] constexpr const std::array<std::pair<Enum, Value>, NumEnumerators> &get_raw_table() const {
			return _mapping;
		}
	protected:
		const std::array<std::pair<Enum, Value>, NumEnumerators> _mapping; ///< Storage for the mapping.
	};


	/// Aligns the given size.
	[[nodiscard]] inline constexpr std::size_t align_size(std::size_t size, std::size_t align) {
		return align * ((size + align - 1) / align);
	}
}
