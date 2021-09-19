#include "lotus/graphics/common.h"

/// \file
/// Implementation of certain common graphics-related functionalities.

#include "lotus/common.h"

namespace lotus::graphics {
	const format_properties &format_properties::get(format fmt) {
		constexpr std::uint8_t o = 0; // zero for visibility
		constexpr static enum_mapping<format, format_properties> table{
			std::pair(format::none,                 zero                                                                      ),
			std::pair(format::d32_float_s8,         format_properties::create_depth_stencil(32, 8, data_type::floatint_point )),
			std::pair(format::d32_float,            format_properties::create_depth_stencil(32, o, data_type::floatint_point )),
			std::pair(format::d24_unorm_s8,         format_properties::create_depth_stencil(24, 8, data_type::unsigned_norm  )),
			std::pair(format::d16_unorm,            format_properties::create_depth_stencil(16, o, data_type::unsigned_norm  )),
			std::pair(format::r8_unorm,             format_properties::create_color( 8,  o,  o,  o, data_type::unsigned_norm )),
			std::pair(format::r8_snorm,             format_properties::create_color( 8,  o,  o,  o, data_type::signed_norm   )),
			std::pair(format::r8_uint,              format_properties::create_color( 8,  o,  o,  o, data_type::unsigned_int  )),
			std::pair(format::r8_sint,              format_properties::create_color( 8,  o,  o,  o, data_type::signed_int    )),
			std::pair(format::r8_unknown,           format_properties::create_color( 8,  o,  o,  o, data_type::unknown       )),
			std::pair(format::r8g8_unorm,           format_properties::create_color( 8,  8,  o,  o, data_type::unsigned_norm )),
			std::pair(format::r8g8_snorm,           format_properties::create_color( 8,  8,  o,  o, data_type::signed_norm   )),
			std::pair(format::r8g8_uint,            format_properties::create_color( 8,  8,  o,  o, data_type::unsigned_int  )),
			std::pair(format::r8g8_sint,            format_properties::create_color( 8,  8,  o,  o, data_type::signed_int    )),
			std::pair(format::r8g8_unknown,         format_properties::create_color( 8,  8,  o,  o, data_type::unknown       )),
			std::pair(format::r8g8b8a8_unorm,       format_properties::create_color( 8,  8,  8,  8, data_type::unsigned_norm )),
			std::pair(format::r8g8b8a8_snorm,       format_properties::create_color( 8,  8,  8,  8, data_type::signed_norm   )),
			std::pair(format::r8g8b8a8_srgb,        format_properties::create_color( 8,  8,  8,  8, data_type::srgb          )),
			std::pair(format::r8g8b8a8_uint,        format_properties::create_color( 8,  8,  8,  8, data_type::unsigned_int  )),
			std::pair(format::r8g8b8a8_sint,        format_properties::create_color( 8,  8,  8,  8, data_type::signed_int    )),
			std::pair(format::r8g8b8a8_unknown,     format_properties::create_color( 8,  8,  8,  8, data_type::unknown       )),
			std::pair(format::r16_unorm,            format_properties::create_color(16,  o,  o,  o, data_type::unsigned_norm )),
			std::pair(format::r16_snorm,            format_properties::create_color(16,  o,  o,  o, data_type::signed_norm   )),
			std::pair(format::r16_uint,             format_properties::create_color(16,  o,  o,  o, data_type::unsigned_int  )),
			std::pair(format::r16_sint,             format_properties::create_color(16,  o,  o,  o, data_type::signed_int    )),
			std::pair(format::r16_float,            format_properties::create_color(16,  o,  o,  o, data_type::floatint_point)),
			std::pair(format::r16_unknown,          format_properties::create_color(16,  o,  o,  o, data_type::unknown       )),
			std::pair(format::r16g16_unorm,         format_properties::create_color(16, 16,  o,  o, data_type::unsigned_norm )),
			std::pair(format::r16g16_snorm,         format_properties::create_color(16, 16,  o,  o, data_type::signed_norm   )),
			std::pair(format::r16g16_uint,          format_properties::create_color(16, 16,  o,  o, data_type::unsigned_int  )),
			std::pair(format::r16g16_sint,          format_properties::create_color(16, 16,  o,  o, data_type::signed_int    )),
			std::pair(format::r16g16_float,         format_properties::create_color(16, 16,  o,  o, data_type::floatint_point)),
			std::pair(format::r16g16_unknown,       format_properties::create_color(16, 16,  o,  o, data_type::unknown       )),
			std::pair(format::r16g16b16a16_unorm,   format_properties::create_color(16, 16, 16, 16, data_type::unsigned_norm )),
			std::pair(format::r16g16b16a16_snorm,   format_properties::create_color(16, 16, 16, 16, data_type::signed_norm   )),
			std::pair(format::r16g16b16a16_uint,    format_properties::create_color(16, 16, 16, 16, data_type::unsigned_int  )),
			std::pair(format::r16g16b16a16_sint,    format_properties::create_color(16, 16, 16, 16, data_type::signed_int    )),
			std::pair(format::r16g16b16a16_float,   format_properties::create_color(16, 16, 16, 16, data_type::floatint_point)),
			std::pair(format::r16g16b16a16_unknown, format_properties::create_color(16, 16, 16, 16, data_type::unknown       )),
			std::pair(format::r32_uint,             format_properties::create_color(32,  o,  o,  o, data_type::unsigned_int  )),
			std::pair(format::r32_sint,             format_properties::create_color(32,  o,  o,  o, data_type::signed_int    )),
			std::pair(format::r32_float,            format_properties::create_color(32,  o,  o,  o, data_type::floatint_point)),
			std::pair(format::r32_unknown,          format_properties::create_color(32,  o,  o,  o, data_type::unknown       )),
			std::pair(format::r32g32_uint,          format_properties::create_color(32, 32,  o,  o, data_type::unsigned_int  )),
			std::pair(format::r32g32_sint,          format_properties::create_color(32, 32,  o,  o, data_type::signed_int    )),
			std::pair(format::r32g32_float,         format_properties::create_color(32, 32,  o,  o, data_type::floatint_point)),
			std::pair(format::r32g32_unknown,       format_properties::create_color(32, 32,  o,  o, data_type::unknown       )),
			std::pair(format::r32g32b32_uint,       format_properties::create_color(32, 32, 32,  o, data_type::unsigned_int  )),
			std::pair(format::r32g32b32_sint,       format_properties::create_color(32, 32, 32,  o, data_type::signed_int    )),
			std::pair(format::r32g32b32_float,      format_properties::create_color(32, 32, 32,  o, data_type::floatint_point)),
			std::pair(format::r32g32b32_unknown,    format_properties::create_color(32, 32, 32,  o, data_type::unknown       )),
			std::pair(format::r32g32b32a32_uint,    format_properties::create_color(32, 32, 32, 32, data_type::unsigned_int  )),
			std::pair(format::r32g32b32a32_sint,    format_properties::create_color(32, 32, 32, 32, data_type::signed_int    )),
			std::pair(format::r32g32b32a32_float,   format_properties::create_color(32, 32, 32, 32, data_type::floatint_point)),
			std::pair(format::r32g32b32a32_unknown, format_properties::create_color(32, 32, 32, 32, data_type::unknown       )),
		};
		return table[fmt];
	}
}
