#pragma once

/// \file
/// Common types and functions.

#include <type_traits>
#include <array>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <optional>
#include <string_view>
#include <source_location>
#include <span>
#include <bit>

namespace lotus {
#ifndef NDEBUG
	constexpr bool is_debugging = true;
#else
	constexpr bool is_debugging = false;
#endif

	/// Calls \p std::abort() if \p value is \p true.
	inline void crash_if(bool value) {
		if (value) {
			std::abort();
		}
	}
	/// Calls \ref crash_if() in debug builds.
	inline void crash_if_debug([[maybe_unused]] bool value) {
		if constexpr (is_debugging) {
			crash_if(value);
		}
	}
	/// \p constexpr version of \ref crash_if().
	inline constexpr void crash_if_constexpr([[maybe_unused]] bool value) {
		assert(!value);
	}

	/// Pauses program execution for debugging.
	inline void pause_for_debugger() {
#ifdef _MSC_VER
		__debugbreak();
#else
		std::abort(); // not implemented
#endif
	}


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
				crash_if_constexpr(static_cast<std::size_t>(_mapping[i].first) != i);
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

	/// Stores a mapping from consecutive bits to mapped values, with additional checking.
	template <
		typename BitMask, typename Value,
		std::size_t NumEnumerators = static_cast<std::size_t>(BitMask::num_enumerators)
	> class bit_mask_mapping {
	public:
		/// Initializes the mapping.
		template <typename ...Args> constexpr bit_mask_mapping(Args &&...args) :
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
				crash_if_constexpr(i != 0);
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


	/// Represents a linear range with beginning and end points. The end points can be either floating point or
	/// integral, and when it's integral the range will be closed at the beginning and open at the end (i.e., [s, e)
	/// will be the range).
	template <typename T> struct linear_range {
		/// Initializes the range to [0, 0).
		constexpr linear_range(zero_t) {
		}
		/// Initializes the range.
		constexpr linear_range(T b, T e) : begin(std::move(b)), end(std::move(e)) {
		}

		/// Returns the intersection of the two ranges.
		[[nodiscard]] constexpr inline static std::optional<linear_range> get_intersection(
			linear_range a, linear_range b
		) {
			if (a.end <= b.begin || a.begin >= b.end) {
				return std::nullopt;
			}
			return linear_range(std::max(a.begin, b.begin), std::min(a.end, b.end));
		}
		/// Returns whether this range fully covers the other range.
		[[nodiscard]] constexpr bool fully_covers(linear_range rhs) const {
			return begin <= rhs.begin && end >= rhs.end;
		}

		/// Returns the length of this range.
		[[nodiscard]] constexpr auto get_length() const {
			return end - begin;
		}
		/// Returns \p false if the end is after the beginning.
		[[nodiscard]] constexpr bool is_empty() const {
			return end <= begin;
		}

		T begin = zero; ///< The beginning of the range.
		T end = zero; ///< The end of the range.
	};
	using linear_float_range  = linear_range<float>;       ///< Shorthand for linear ranges for \p float.
	using linear_size_t_range = linear_range<std::size_t>; ///< Shorthand for linear range for \p std::size_t.


	/// Console output utilities.
	namespace console {
		/// Console colors.
		enum class color {
			black        = 30,
			dark_red     = 31,
			dark_green   = 32,
			dark_orange  = 33,
			dark_blue    = 34,
			dark_magenta = 35,
			dark_cyan    = 36,
			light_gray   = 37,
			dark_gray    = 90,
			red          = 91,
			green        = 92,
			orange       = 93,
			blue         = 94,
			magenta      = 95,
			cyan         = 96,
			white        = 97,
		};

		/// Sets the foreground color of console output.
		inline void set_foreground_color(color c, FILE *fout = nullptr) {
			fprintf(fout ? fout : stdout, "\033[%dm", static_cast<int>(c));
		}
		/// Resets console text color.
		inline void reset_color(FILE *fout = nullptr) {
			fprintf(fout ? fout : stdout, "\033[0m");
		}
	}
}
