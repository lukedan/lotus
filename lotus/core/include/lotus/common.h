#pragma once

/// \file
/// Common types and functions.

#include <type_traits>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>
#include <source_location>
#include <span>
#include <bit>

#if defined(__clang__) && defined(_MSC_VER)
// clang-cl does not implement no_unique_address
#	define no_unique_address msvc::no_unique_address
#endif

#include "types.h"

namespace lotus {
#ifndef NDEBUG
	constexpr bool is_debugging = true;
#else
	constexpr bool is_debugging = false;
#endif

	/// Pauses program execution for debugging.
	inline void pause_for_debugger() {
#if defined(_MSC_VER)
		__debugbreak();
#elif defined(__clang__)
		__builtin_debugtrap();
#else
		std::abort(); // not implemented
#endif
	}

	/// Calls \p std::abort() if \p value is \p true.
	constexpr void crash_if(bool value) {
		if (std::is_constant_evaluated()) {
			assert(!value);
		} else {
			if (value) {
				pause_for_debugger();
			}
		}
	}
	/// Calls \ref crash_if() in debug builds.
	constexpr void crash_if_debug([[maybe_unused]] bool value) {
		if constexpr (is_debugging) {
			crash_if(value);
		}
	}


	/// A type indicating a specific object should not be initialized.
	struct uninitialized_t {
		/// Implicit conversion to arithmetic types and pointers.
		template <
			typename T,
			std::enable_if_t<std::is_arithmetic_v<T> || std::is_pointer_v<T> || std::is_enum_v<T>, int> = 0
		> operator T() const {
#if defined(__clang__)
			return __builtin_nondeterministic_value(T{});
#else
			return T{};
#endif
		}
	};
	/// A type indicating a specific object should be zero-initialized.
	struct zero_t {
		/// Implicit conversion to arithmetic types and pointers.
		template <
			typename T,
			std::enable_if_t<std::is_arithmetic_v<T> || std::is_pointer_v<T> || std::is_enum_v<T>, int> = 0
		> consteval operator T() const {
			return static_cast<T>(0);
		}
	};
	constexpr inline uninitialized_t uninitialized; ///< An instance of \ref uninitialized_t.
	constexpr inline zero_t zero; ///< An instance of \ref zero_t.


	/// The minimum-sized type that is able to hold the given value.
	template <u64 Value> struct minimum_unsigned_type {
		/// The type.
		using type = std::conditional_t<
			(Value < std::numeric_limits<u8>::max()), u8,
			std::conditional_t<
				(Value < std::numeric_limits<u16>::max()), u16,
				std::conditional_t<
					(Value < std::numeric_limits<u32>::max()), u32,
					u64
				>
			>
		>;
	};
	/// Shorthand for \ref minimum_unsigned_type::type.
	template <u64 Value> using minimum_unsigned_type_t = minimum_unsigned_type<Value>::type;


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
		template <T First, T ...Others> [[nodiscard]] constexpr static T get(usize i) {
			if constexpr (sizeof...(Others) == 0) {
				crash_if(i != 0);
				return First;
			} else {
				return i == 0 ? First : get<Others...>(i - 1);
			}
		}
	};
	namespace _details {
		/// Implementation of \ref filled_array().
		template <
			usize Size, typename T, usize ...Is
		> [[nodiscard]] constexpr std::array<T, Size> filled_array_impl(const T &val, std::index_sequence<Is...>) {
			return { { (static_cast<void>(Is), val)... } };
		}
	}
	/// Creates an array filled with the given element.
	template <usize Size, typename T> [[nodiscard]] constexpr std::array<T, Size> filled_array(const T &val) {
		return _details::filled_array_impl<Size, T>(val, std::make_index_sequence<Size>());
	}


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
	template <typename T, typename Hash = std::hash<T>> [[nodiscard]] constexpr usize compute_hash(
		const T &obj, const Hash &hash = Hash()
	) {
		return hash(obj);
	}

	/// Combines the result of two hash functions.
	[[nodiscard]] constexpr usize hash_combine(usize hash1, usize hash2) {
		return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
	}
	/// Shorthand for combining a sequence of hashes, with each hash being combined with the result of all previous
	/// hashes with \ref hash_combine().
	[[nodiscard]] constexpr usize hash_combine(std::span<const usize> hashes) {
		if (hashes.empty()) {
			return 0;
		}
		usize result = hashes[0];
		for (auto it = hashes.begin() + 1; it != hashes.end(); ++it) {
			result = hash_combine(result, *it);
		}
		return result;
	}
	/// \overload
	[[nodiscard]] constexpr usize hash_combine(std::initializer_list<usize> hashes) {
		return hash_combine({ hashes.begin(), hashes.end() });
	}

	/// Hashes a \p std::source_location.
	[[nodiscard]] inline usize hash_source_location(const std::source_location &loc) {
		return hash_combine({
			compute_hash(loc.file_name()),
			compute_hash(loc.line()),
			compute_hash(loc.column()),
		});
	}

	/// The FNV-1a hash.
	namespace fnv1a {
		/// Constants for different hash sizes.
		template <typename T> struct constants;
		/// Constants for 32-bit FNV-1a hash.
		template <> struct constants<u32> {
			constexpr static u32 prime  = 16777619u;   ///< The multiplier.
			constexpr static u32 offset = 2166136261u; ///< The initial offset.
		};
		/// Constants for 64-bit FNV-1a hash.
		template <> struct constants<u64> {
			constexpr static u64 prime  = 1099511628211ULL;        ///< The multiplier.
			constexpr static u64 offset = 14695981039346656037ULL; ///< The initial offset.
		};

		/// Hashes the given range of bytes using FNV-1a.
		template <typename T = u32> [[nodiscard]] constexpr T hash_bytes(std::span<const std::byte> data) {
			T hash = constants<T>::offset;
			for (std::byte b : data) {
				hash ^= static_cast<T>(b);
				hash *= constants<T>::prime;
			}
			return hash;
		}
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
	using linear_float_range = linear_range<float>; ///< Shorthand for linear ranges for \p float.
	using linear_usize_range = linear_range<usize>; ///< Shorthand for linear range for \ref usize.


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
