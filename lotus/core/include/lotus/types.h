#pragma once

/// \file
/// Definitions of shorthands for primitive types.

#include <cstdint>
#include <cstddef>

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
	}
}
