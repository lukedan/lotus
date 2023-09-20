#pragma once

/// \file
/// Enum utilities.

#include <cstddef>
#include <type_traits>
#include <array>
#include <bit>

#include "common.h"

namespace lotus::enums {
	/// Stores a mapping from consecutive zero-based enum values to mapped values, with additional checks.
	template <
		typename Enum, typename Value, std::size_t NumEnumerators = static_cast<std::size_t>(Enum::num_enumerators)
	> class sequential_mapping {
	public:
		using storage = std::array<std::pair<Enum, Value>, NumEnumerators>; ///< Storage type.

		/// Initializes the mapping.
		template <typename ...Args> constexpr sequential_mapping(Args &&...args) :
			_mapping{ { std::forward<Args>(args)... } } {
			static_assert(sizeof...(args) == NumEnumerators, "Incorrect number of entries for enum mapping.");
			for (std::size_t i = 0; i < NumEnumerators; ++i) {
				crash_if_constexpr(std::to_underlying(_mapping[i].first) != i);
			}
		}

		/// Retrieves the mapping for the given value.
		[[nodiscard]] constexpr const Value &operator[](Enum v) const {
			return _mapping[std::to_underlying(v)].second;
		}
		/// Returns the entire table.
		[[nodiscard]] constexpr const storage &get_raw_table() const {
			return _mapping;
		}
	private:
		const storage _mapping; ///< Storage for the mapping.
	};

	/// Stores a mapping from consecutive zero-based enum values to dynamic values.
	template <
		typename Enum, typename Value, std::size_t NumEnumerators = static_cast<std::size_t>(Enum::num_enumerators)
	> class dynamic_sequential_mapping {
	public:
		using storage = std::array<Value, NumEnumerators>; ///< Storage type.

		/// Initializes all values.
		template <typename ...Args> constexpr dynamic_sequential_mapping(Args &&...args) :
			_data{ { std::forward<Args>(args)... } } {
		}
		/// Initializes the array directly.
		explicit constexpr dynamic_sequential_mapping(storage s) : _data(std::move(s)) {
		}
		/// Returns an object where all enums map to the given value.
		[[nodiscard]] constexpr inline static dynamic_sequential_mapping filled(const Value &val) {
			return dynamic_sequential_mapping(filled_array<NumEnumerators, Value>(val));
		}

		/// Returns the value that corresponds to the given enumerator.
		[[nodiscard]] constexpr Value &operator[](Enum v) {
			return _data[std::to_underlying(v)];
		}
		/// \overload
		[[nodiscard]] constexpr const Value &operator[](Enum v) const {
			return _data[std::to_underlying(v)];
		}

		/// Returns the array of values.
		[[nodiscard]] constexpr storage &get_storage() {
			return _data;
		}
		/// \overload
		[[nodiscard]] constexpr const storage &get_storage() const {
			return _data;
		}
	private:
		storage _data;
	};


	/// Indicates if an enum type is treated as a bit mask type. This decides if it can be used with the following
	/// bitwise operators.
	template <typename> struct is_bit_mask : public std::false_type {
	};
	/// Shorthand for \ref is_bit_mask.
	template <typename T> constexpr static bool is_bit_mask_v = is_bit_mask<T>::value;
}

// these are put in the global scope so that all code can access these
/// Bitwise and for enum classes.
template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enums::is_bit_mask_v<Enum>, Enum
> operator&(Enum lhs, Enum rhs) {
	return static_cast<Enum>(std::to_underlying(lhs) & std::to_underlying(rhs));
}
/// Bitwise or for enum classes.
template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enums::is_bit_mask_v<Enum>, Enum
> operator|(Enum lhs, Enum rhs) {
	return static_cast<Enum>(std::to_underlying(lhs) | std::to_underlying(rhs));
}
/// Bitwise xor for enum classes.
template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enums::is_bit_mask_v<Enum>, Enum
> operator^(Enum lhs, Enum rhs) {
	return static_cast<Enum>(std::to_underlying(lhs) ^ std::to_underlying(rhs));
}
/// Bitwise not for enum classes.
template <typename Enum> [[nodiscard]] inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enums::is_bit_mask_v<Enum>, Enum
> operator~(Enum v) {
	return static_cast<Enum>(~std::to_underlying(v));
}

/// Bitwise and for enum classes.
template <typename Enum> inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enums::is_bit_mask_v<Enum>, Enum&
> operator&=(Enum &lhs, Enum rhs) {
	return lhs = lhs & rhs;
}
/// Bitwise or for enum classes.
template <typename Enum> inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enums::is_bit_mask_v<Enum>, Enum&
> operator|=(Enum &lhs, Enum rhs) {
	return lhs = lhs | rhs;
}
/// Bitwise xor for enum classes.
template <typename Enum> inline constexpr std::enable_if_t<
	std::is_enum_v<Enum> && lotus::enums::is_bit_mask_v<Enum>, Enum&
> operator^=(Enum &lhs, Enum rhs) {
	return lhs = lhs ^ rhs;
}


namespace lotus::enums::bit_mask {
	/// Tests if a bit mask is zero.
	template <typename Enum> [[nodiscard]] inline constexpr bool is_empty(Enum v) {
		return v == static_cast<Enum>(0);
	}

	/// Tests whether \p value contains all bits in \p bits.
	template <typename Enum> [[nodiscard]] inline constexpr bool contains_all(Enum value, Enum bits) {
		return (value & bits) == bits;
	}
	/// Tests whether \p value contains any bits in \p bits.
	template <typename Enum> [[nodiscard]] inline constexpr bool contains_any(Enum value, Enum bits) {
		return !is_empty(value & bits);
	}
	/// Tests that \p value contains no bits in \p bits.
	template <typename Enum> [[nodiscard]] inline constexpr bool contains_none(Enum value, Enum bits) {
		return !contains_any(value, bits);
	}
	/// Tests whether \p value contains the given single bit.
	template <auto Bit, typename Enum = decltype(Bit)> [[nodiscard]] inline constexpr bool contains(Enum value) {
		static_assert(std::popcount(std::to_underlying(Bit)) == 1, "Value contains more than 1 bits");
		return contains_any<Enum>(value, Bit);
	}


	/// Stores a mapping from one bit mask type to another.
	template <
		typename BitMask, typename Value,
		std::size_t NumEnumerators = static_cast<std::size_t>(BitMask::num_enumerators)
	> class mapping {
	public:
		/// Initializes the mapping.
		template <typename ...Args> constexpr mapping(Args &&...args) :
			_mapping{ { std::forward<Args>(args)... } } {
			static_assert(sizeof...(args) == NumEnumerators, "Incorrect number of entries for bit mask mapping.");
			for (std::size_t i = 0; i < NumEnumerators; ++i) {
				crash_if_constexpr(_mapping[i].first != static_cast<BitMask>(1 << i));
			}
		}

		/// Returns the bitwise or of all values corresponding to all bits in the given bit mask.
		[[nodiscard]] constexpr Value get_union(BitMask m) const {
			using _src_ty = std::underlying_type_t<BitMask>;
			using _dst_ty = std::underlying_type_t<Value>;

			auto value = static_cast<_src_ty>(m);
			auto result = static_cast<_dst_ty>(0);
			while (value != 0) {
				int bit = std::countr_zero(value);
				crash_if_constexpr(bit >= NumEnumerators);
				result |= static_cast<_dst_ty>(_mapping[bit].second);
				value ^= static_cast<_src_ty>(1ull << bit);
			}
			return static_cast<Value>(result);
		}
	protected:
		const std::array<std::pair<BitMask, Value>, NumEnumerators> _mapping; ///< Storage for the mapping.
	};
}

namespace lotus::bit_mask {
	using namespace enums::bit_mask;
}
