#pragma once

/// \file
/// Definitions of shorthands for primitive types.

#include <cstdint>
#include <cstddef>
#if __has_include(<stdfloat>)
#	include <stdfloat>
#endif

namespace lotus {
	/// Short definitions of all primitive type names.
	inline namespace types {
		using u8  = std::uint8_t;  ///< 8-bit unsigned integral type.
		using u16 = std::uint16_t; ///< 16-bit unsigned integral type.
		using u32 = std::uint32_t; ///< 32-bit unsigned integral type.
		using u64 = std::uint64_t; ///< 64-bit unsigned integral type.
		using usize = std::size_t; ///< Size type.

		using i8  = std::int8_t;  ///< 8-bit signed integral type.
		using i16 = std::int16_t; ///< 16-bit signed integral type.
		using i32 = std::int32_t; ///< 32-bit signed integral type.
		using i64 = std::int64_t; ///< 64-bit signed integral type.

		/// 32-bit floating point types.
#ifdef __STDCPP_FLOAT32_T__
		using f32 = std::float32_t;
#else
		using f32 = float;
		static_assert(sizeof(f32) == sizeof(u32), "Expecting float to be 32 bits wide");
#endif
		/// 64-bit floating point types.
#ifdef __STDCPP_FLOAT64_T__
		using f64 = std::float64_t;
#else
		using f64 = double;
		static_assert(sizeof(f64) == sizeof(u64), "Expecting double to be 64 bits wide");
#endif
	}
}
