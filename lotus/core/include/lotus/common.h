#pragma once

/// \file
/// Common types and functions.

#include <type_traits>
#include <array>
#include <cassert>
#include <string_view>
#include <source_location>
#include <span>

namespace lotus {
#ifndef NDEBUG
	constexpr bool is_debugging = true;
#else
	constexpr bool is_debugging = false;
#endif

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
		> consteval operator T() const {
			return static_cast<T>(0);
		}
	};
	constexpr inline const uninitialized_t uninitialized; ///< An instance of \ref uninitialized_t.
	constexpr inline const zero_t zero; ///< An instance of \ref zero_t.


	/// Indicates if all following bitwise operators are enabled for a type. Enum classes can create specializations
	/// to enable them.
	template <typename> struct enable_enum_bitwise_operators : public std::false_type {
	};
	/// Shorthand for \ref enable_enum_bitwise_operators.
	template <typename T> constexpr static bool enable_enum_bitwise_operators_v =
		enable_enum_bitwise_operators<T>::value;
}
// these are put in the global scope so that all code can access these
/// Bitwise and for enum classes.
template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enable_enum_bitwise_operators_v<Enum>, Enum
> operator&(Enum lhs, Enum rhs) {
	using _base = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<_base>(lhs) & static_cast<_base>(rhs));
}
/// Bitwise or for enum classes.
template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enable_enum_bitwise_operators_v<Enum>, Enum
> operator|(Enum lhs, Enum rhs) {
	using _base = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<_base>(lhs) | static_cast<_base>(rhs));
}
/// Bitwise xor for enum classes.
template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enable_enum_bitwise_operators_v<Enum>, Enum
> operator^(Enum lhs, Enum rhs) {
	using _base = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<_base>(lhs) ^ static_cast<_base>(rhs));
}
/// Bitwise not for enum classes.
template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enable_enum_bitwise_operators_v<Enum>, Enum
> operator~(Enum v) {
	using _base = std::underlying_type_t<Enum>;
	return static_cast<Enum>(~static_cast<_base>(v));
}

/// Bitwise and for enum classes.
template <typename Enum> inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enable_enum_bitwise_operators_v<Enum>, Enum&
> operator&=(Enum &lhs, Enum rhs) {
	return lhs = lhs & rhs;
}
/// Bitwise or for enum classes.
template <typename Enum> inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enable_enum_bitwise_operators_v<Enum>, Enum&
> operator|=(Enum &lhs, Enum rhs) {
	return lhs = lhs | rhs;
}
/// Bitwise xor for enum classes.
template <typename Enum> inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enable_enum_bitwise_operators_v<Enum>, Enum&
> operator^=(Enum &lhs, Enum rhs) {
	return lhs = lhs ^ rhs;
}

namespace lotus {
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


	/// Used to obtain certain attributes of member pointers.
	template <typename> struct member_pointer_type_traits;
	/// Specialization for member object pointers.
	template <typename Owner, typename Val> struct member_pointer_type_traits<Val Owner::*> {
		using value_type = Val; ///< The type of the pointed-to object.
		using owner_type = Owner; ///< The type of the owner.
	};
	/// Shorthand for \ref member_pointer_type_traits using a member pointer instance.
	template <auto MemberPtr> using member_pointer_traits = member_pointer_type_traits<decltype(MemberPtr)>;

	/// Template parameter pack utilities.
	template <typename T> struct parameter_pack {
	public:
		/// Dynamically retrieves the i-th element.
		template <T First, T ...Others> [[nodiscard]] constexpr inline static T get(std::size_t i) {
			if constexpr (sizeof...(Others) == 0) {
				assert(i == 0);
				return First;
			} else {
				return i == 0 ? First : get<Others...>(i - 1);
			}
		}
	};


	/// Tests the equality of two \p std::source_location objects.
	[[nodiscard]] inline bool source_locations_equal(
		const std::source_location &lhs, const std::source_location &rhs
	) {
		if (lhs.line() != rhs.line() || lhs.column() != rhs.column()) {
			return false;
		}
		if (lhs.file_name() != rhs.file_name()) {
			return std::strcmp(lhs.file_name(), rhs.file_name()) == 0;
		}
		return true;
	}

	/// Shorthand for creating a hash object and then hashing the given object.
	template <typename T, typename Hash = std::hash<T>> [[nodiscard]] inline constexpr std::size_t compute_hash(
		const T &obj, const Hash &hash = Hash()
	) {
		return hash(obj);
	}

	/// Combines the result of two hash functions.
	[[nodiscard]] constexpr inline std::size_t hash_combine(std::size_t hash1, std::size_t hash2) {
		return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
	}
	/// Shorthand for combining a sequence of hashes, with each hash being combined with the result of all previous
	/// hashes with \ref hash_combine().
	[[nodiscard]] constexpr inline std::size_t hash_combine(std::span<const std::size_t> hashes) {
		if (hashes.empty()) {
			return 0;
		}
		std::size_t result = hashes[0];
		for (auto it = hashes.begin() + 1; it != hashes.end(); ++it) {
			result = hash_combine(result, *it);
		}
		return result;
	}
	/// \overload
	[[nodiscard]] constexpr inline std::size_t hash_combine(std::initializer_list<std::size_t> hashes) {
		return hash_combine({ hashes.begin(), hashes.end() });
	}

	/// Hashes a \p std::source_location.
	[[nodiscard]] inline std::size_t hash_source_location(const std::source_location &loc) {
		return hash_combine({
			compute_hash(loc.file_name()),
			compute_hash(loc.line()),
			compute_hash(loc.column()),
		});
	}
}
