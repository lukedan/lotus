#pragma once

/// \file
/// Common graphics-related structures. This is the only file that can be included by backends.

#include <cassert>

namespace lotus::graphics {
	/// Properties of an adapter.
	struct adapter_properties {

	};

	/// Data type for pixels.
	enum class pixel_type : std::uint8_t {
		float_bit = 0, ///< Bit pattern that indicates that the type is floating point.
		int_bit = 1, ///< Bit pattern that indicates that the type is integer.
		normalized_bit = 2, ///< Bit pattern that indicates this type is normalized.
		srgb_bit = 3, ///< Bit pattern that indicates this type is unsigned normalized sRGB.

		signed_bit = 4, ///< The bit that indicates that the type is signed.


		none = 0, ///< No specific type.

		floating_point      = float_bit      | signed_bit, ///< Floating point number.
		unsigned_integer    = int_bit,                     ///< Unsigned integer.
		signed_integer      = int_bit        | signed_bit, ///< Signed integer.
		unsigned_normalized = normalized_bit,              ///< Unsigned value normalized to [0, 1].
		signed_normalized   = normalized_bit | signed_bit, ///< Signed value normalized to [0, 1].
		srgb                = srgb_bit,                    ///< Unsigned sRGB value normalized to [0, 1].
	};

	/// The number of bits used to store the number of bits for a channel.
	constexpr std::size_t pixel_format_channel_bit_count = 6;
	namespace _details {
		/// Creates a pixel format enum from the given parameters;
		[[nodiscard]] inline constexpr uint32_t create_pixel_format(
			std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha, pixel_type type
		) {
			constexpr std::uint32_t _bit_count_mask = (1 << pixel_format_channel_bit_count) - 1;
			assert((red & _bit_count_mask) == red);
			assert((green & _bit_count_mask) == green);
			assert((blue & _bit_count_mask) == blue);
			assert((alpha & _bit_count_mask) == alpha);
			return
				static_cast<uint32_t>(red) |
				(static_cast<uint32_t>(green) << pixel_format_channel_bit_count) |
				(static_cast<uint32_t>(blue) << (2 * pixel_format_channel_bit_count)) |
				(static_cast<uint32_t>(alpha) << (3 * pixel_format_channel_bit_count)) |
				static_cast<uint32_t>(type) << (4 * pixel_format_channel_bit_count);
		}
	}
	/// The format of a pixel.
	enum class pixel_format : std::uint32_t {
		r8g8b8a8_unorm       = _details::create_pixel_format(8,  8,  8,  8,  pixel_type::unsigned_normalized),
		r8g8b8a8_snorm       = _details::create_pixel_format(8,  8,  8,  8,  pixel_type::signed_normalized),
		r8g8b8a8_srgb        = _details::create_pixel_format(8,  8,  8,  8,  pixel_type::srgb),
		r8g8b8a8_uint        = _details::create_pixel_format(8,  8,  8,  8,  pixel_type::unsigned_integer),
		r8g8b8a8_sint        = _details::create_pixel_format(8,  8,  8,  8,  pixel_type::signed_integer),
		r8g8b8a8_unknown     = _details::create_pixel_format(8,  8,  8,  8,  pixel_type::none),

		r16g16b16a16_unorm   = _details::create_pixel_format(16, 16, 16, 16, pixel_type::unsigned_normalized),
		r16g16b16a16_snorm   = _details::create_pixel_format(16, 16, 16, 16, pixel_type::signed_normalized),
		r16g16b16a16_srgb    = _details::create_pixel_format(16, 16, 16, 16, pixel_type::srgb),
		r16g16b16a16_uint    = _details::create_pixel_format(16, 16, 16, 16, pixel_type::unsigned_integer),
		r16g16b16a16_sint    = _details::create_pixel_format(16, 16, 16, 16, pixel_type::signed_integer),
		r16g16b16a16_float   = _details::create_pixel_format(16, 16, 16, 16, pixel_type::floating_point),
		r16g16b16a16_unknown = _details::create_pixel_format(16, 16, 16, 16, pixel_type::none),
	};
	/// \overload
	[[nodiscard]] inline constexpr pixel_format create_pixel_format(
		std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha, pixel_type type
	) {
		return static_cast<pixel_format>(red, green, blue, alpha, type);
	}
}
