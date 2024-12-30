#include "lotus/gpu/common.h"

/// \file
/// Implementation of certain common graphics-related functionalities.

#include "lotus/common.h"

namespace lotus::gpu {
	constexpr static enums::sequential_mapping<backend_type, std::u8string_view> _backend_name_table{
		std::pair(backend_type::directx12, u8"DirectX 12"),
		std::pair(backend_type::vulkan,    u8"Vulkan"    ),
		std::pair(backend_type::metal,     u8"Metal"     ),
	};
	std::u8string_view get_backend_name(backend_type ty) {
		return _backend_name_table[ty];
	}

	constexpr static std::uint8_t o = 0; // zero for visibility
	constexpr static cvec2u8 _bc7_block_size = cvec2i(4, 4).into<std::uint8_t>();
	constexpr static enums::sequential_mapping<format, format_properties> _format_property_table{
		std::pair(format::none,               zero),
		std::pair(format::d32_float_s8,       format_properties::create_depth_stencil(32, 8, format_properties::data_type::floating_point)),
		std::pair(format::d32_float,          format_properties::create_depth_stencil(32, o, format_properties::data_type::floating_point)),
		std::pair(format::d24_unorm_s8,       format_properties::create_depth_stencil(24, 8, format_properties::data_type::unsigned_norm )),
		std::pair(format::d16_unorm,          format_properties::create_depth_stencil(16, o, format_properties::data_type::unsigned_norm )),
		std::pair(format::r8_unorm,           format_properties::create_uncompressed_color( 8,  o,  o,  o, format_properties::data_type::unsigned_norm,  format_properties::fragment_contents::rgba)),
		std::pair(format::r8_snorm,           format_properties::create_uncompressed_color( 8,  o,  o,  o, format_properties::data_type::signed_norm,    format_properties::fragment_contents::rgba)),
		std::pair(format::r8_uint,            format_properties::create_uncompressed_color( 8,  o,  o,  o, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r8_sint,            format_properties::create_uncompressed_color( 8,  o,  o,  o, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8_unorm,         format_properties::create_uncompressed_color( 8,  8,  o,  o, format_properties::data_type::unsigned_norm,  format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8_snorm,         format_properties::create_uncompressed_color( 8,  8,  o,  o, format_properties::data_type::signed_norm,    format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8_uint,          format_properties::create_uncompressed_color( 8,  8,  o,  o, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8_sint,          format_properties::create_uncompressed_color( 8,  8,  o,  o, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8b8a8_unorm,     format_properties::create_uncompressed_color( 8,  8,  8,  8, format_properties::data_type::unsigned_norm,  format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8b8a8_snorm,     format_properties::create_uncompressed_color( 8,  8,  8,  8, format_properties::data_type::signed_norm,    format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8b8a8_srgb,      format_properties::create_uncompressed_color( 8,  8,  8,  8, format_properties::data_type::srgb,           format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8b8a8_uint,      format_properties::create_uncompressed_color( 8,  8,  8,  8, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r8g8b8a8_sint,      format_properties::create_uncompressed_color( 8,  8,  8,  8, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::b8g8r8a8_unorm,     format_properties::create_uncompressed_color( 8,  8,  8,  8, format_properties::data_type::unsigned_norm,  format_properties::fragment_contents::bgra)),
		std::pair(format::b8g8r8a8_srgb,      format_properties::create_uncompressed_color( 8,  8,  8,  8, format_properties::data_type::srgb,           format_properties::fragment_contents::bgra)),
		std::pair(format::r16_unorm,          format_properties::create_uncompressed_color(16,  o,  o,  o, format_properties::data_type::unsigned_norm,  format_properties::fragment_contents::rgba)),
		std::pair(format::r16_snorm,          format_properties::create_uncompressed_color(16,  o,  o,  o, format_properties::data_type::signed_norm,    format_properties::fragment_contents::rgba)),
		std::pair(format::r16_uint,           format_properties::create_uncompressed_color(16,  o,  o,  o, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r16_sint,           format_properties::create_uncompressed_color(16,  o,  o,  o, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r16_float,          format_properties::create_uncompressed_color(16,  o,  o,  o, format_properties::data_type::floating_point, format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16_unorm,       format_properties::create_uncompressed_color(16, 16,  o,  o, format_properties::data_type::unsigned_norm,  format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16_snorm,       format_properties::create_uncompressed_color(16, 16,  o,  o, format_properties::data_type::signed_norm,    format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16_uint,        format_properties::create_uncompressed_color(16, 16,  o,  o, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16_sint,        format_properties::create_uncompressed_color(16, 16,  o,  o, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16_float,       format_properties::create_uncompressed_color(16, 16,  o,  o, format_properties::data_type::floating_point, format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16b16a16_unorm, format_properties::create_uncompressed_color(16, 16, 16, 16, format_properties::data_type::unsigned_norm,  format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16b16a16_snorm, format_properties::create_uncompressed_color(16, 16, 16, 16, format_properties::data_type::signed_norm,    format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16b16a16_uint,  format_properties::create_uncompressed_color(16, 16, 16, 16, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16b16a16_sint,  format_properties::create_uncompressed_color(16, 16, 16, 16, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r16g16b16a16_float, format_properties::create_uncompressed_color(16, 16, 16, 16, format_properties::data_type::floating_point, format_properties::fragment_contents::rgba)),
		std::pair(format::r32_uint,           format_properties::create_uncompressed_color(32,  o,  o,  o, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r32_sint,           format_properties::create_uncompressed_color(32,  o,  o,  o, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r32_float,          format_properties::create_uncompressed_color(32,  o,  o,  o, format_properties::data_type::floating_point, format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32_uint,        format_properties::create_uncompressed_color(32, 32,  o,  o, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32_sint,        format_properties::create_uncompressed_color(32, 32,  o,  o, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32_float,       format_properties::create_uncompressed_color(32, 32,  o,  o, format_properties::data_type::floating_point, format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32b32_uint,     format_properties::create_uncompressed_color(32, 32, 32,  o, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32b32_sint,     format_properties::create_uncompressed_color(32, 32, 32,  o, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32b32_float,    format_properties::create_uncompressed_color(32, 32, 32,  o, format_properties::data_type::floating_point, format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32b32a32_uint,  format_properties::create_uncompressed_color(32, 32, 32, 32, format_properties::data_type::unsigned_int,   format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32b32a32_sint,  format_properties::create_uncompressed_color(32, 32, 32, 32, format_properties::data_type::signed_int,     format_properties::fragment_contents::rgba)),
		std::pair(format::r32g32b32a32_float, format_properties::create_uncompressed_color(32, 32, 32, 32, format_properties::data_type::floating_point, format_properties::fragment_contents::rgba)),
		std::pair(format::bc1_unorm,          format_properties::create_compressed(8,  _bc7_block_size, format_properties::data_type::unsigned_norm,           format_properties::fragment_contents::bc1 )),
		std::pair(format::bc1_srgb,           format_properties::create_compressed(8,  _bc7_block_size, format_properties::data_type::srgb,                    format_properties::fragment_contents::bc1 )),
		std::pair(format::bc2_unorm,          format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::unsigned_norm,           format_properties::fragment_contents::bc2 )),
		std::pair(format::bc2_srgb,           format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::srgb,                    format_properties::fragment_contents::bc2 )),
		std::pair(format::bc3_unorm,          format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::unsigned_norm,           format_properties::fragment_contents::bc3 )),
		std::pair(format::bc3_srgb,           format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::srgb,                    format_properties::fragment_contents::bc3 )),
		std::pair(format::bc4_unorm,          format_properties::create_compressed(8,  _bc7_block_size, format_properties::data_type::unsigned_norm,           format_properties::fragment_contents::bc4 )),
		std::pair(format::bc4_snorm,          format_properties::create_compressed(8,  _bc7_block_size, format_properties::data_type::signed_norm,             format_properties::fragment_contents::bc4 )),
		std::pair(format::bc5_unorm,          format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::unsigned_norm,           format_properties::fragment_contents::bc5 )),
		std::pair(format::bc5_snorm,          format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::signed_norm,             format_properties::fragment_contents::bc5 )),
		std::pair(format::bc6h_f16,           format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::floating_point,          format_properties::fragment_contents::bc6h)),
		std::pair(format::bc6h_uf16,          format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::unsigned_floating_point, format_properties::fragment_contents::bc6h)),
		std::pair(format::bc7_unorm,          format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::unsigned_norm,           format_properties::fragment_contents::bc7 )),
		std::pair(format::bc7_srgb,           format_properties::create_compressed(16, _bc7_block_size, format_properties::data_type::srgb,                    format_properties::fragment_contents::bc7 )),
	};

	const format_properties &format_properties::get(format fmt) {
		return _format_property_table[fmt];
	}

	format format_properties::find_exact_rgba(
		std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, data_type type
	) {
		format result = format::none;
		for (const auto [value, key] : _format_property_table.get_raw_table()) {
			if (key.depth_bits > 0 || key.stencil_bits > 0) {
				continue;
			}
			if (
				key.red_bits == r &&
				key.green_bits == g &&
				key.blue_bits == b &&
				key.alpha_bits == a &&
				key.type == type &&
				key.contents == fragment_contents::rgba
			) {
				assert(result == format::none); // duplicate formats?
				result = value;
			}
		}
		return result;
	}
}


namespace lotus::enums::bit_mask {
	const mapping<gpu::image_aspect_mask, std::u8string_view> name_mapping<gpu::image_aspect_mask>::value{
		std::pair(gpu::image_aspect_mask::color,   u8"color"  ),
		std::pair(gpu::image_aspect_mask::depth,   u8"depth"  ),
		std::pair(gpu::image_aspect_mask::stencil, u8"stencil"),
	};
}


namespace lotus::enums {
	const sequential_mapping<gpu::image_layout, std::u8string_view> name_mapping<gpu::image_layout>::value{
		std::pair(gpu::image_layout::undefined,                u8"undefined"               ),
		std::pair(gpu::image_layout::general,                  u8"general"                 ),
		std::pair(gpu::image_layout::copy_source,              u8"copy_source"             ),
		std::pair(gpu::image_layout::copy_destination,         u8"copy_destination"        ),
		std::pair(gpu::image_layout::present,                  u8"present"                 ),
		std::pair(gpu::image_layout::color_render_target,      u8"color_render_target"     ),
		std::pair(gpu::image_layout::depth_stencil_read_only,  u8"depth_stencil_read_only" ),
		std::pair(gpu::image_layout::depth_stencil_read_write, u8"depth_stencil_read_write"),
		std::pair(gpu::image_layout::shader_read_only,         u8"shader_read_only"        ),
		std::pair(gpu::image_layout::shader_read_write,        u8"shader_read_write"       ),
	};
}


namespace lotus::enums::bit_mask {
	const mapping<gpu::synchronization_point_mask, std::u8string_view>
		name_mapping<gpu::synchronization_point_mask>::value{
		std::pair(gpu::synchronization_point_mask::all,                          u8"all"                         ),
		std::pair(gpu::synchronization_point_mask::all_graphics,                 u8"all_graphics"                ),
		std::pair(gpu::synchronization_point_mask::index_input,                  u8"index_input"                 ),
		std::pair(gpu::synchronization_point_mask::vertex_input,                 u8"vertex_input"                ),
		std::pair(gpu::synchronization_point_mask::vertex_shader,                u8"vertex_shader"               ),
		std::pair(gpu::synchronization_point_mask::pixel_shader,                 u8"pixel_shader"                ),
		std::pair(gpu::synchronization_point_mask::depth_stencil_read_write,     u8"depth_stencil_read_write"    ),
		std::pair(gpu::synchronization_point_mask::render_target_read_write,     u8"render_target_read_write"    ),
		std::pair(gpu::synchronization_point_mask::compute_shader,               u8"compute_shader"              ),
		std::pair(gpu::synchronization_point_mask::raytracing,                   u8"raytracing"                  ),
		std::pair(gpu::synchronization_point_mask::copy,                         u8"copy"                        ),
		std::pair(gpu::synchronization_point_mask::acceleration_structure_build, u8"acceleration_structure_build"),
		std::pair(gpu::synchronization_point_mask::acceleration_structure_copy,  u8"acceleration_structure_copy" ),
		std::pair(gpu::synchronization_point_mask::cpu_access,                   u8"cpu_access"                  ),
	};


	const mapping<gpu::image_access_mask, std::u8string_view> name_mapping<gpu::image_access_mask>::value{
		std::pair(gpu::image_access_mask::copy_source,              u8"copy_source"              ),
		std::pair(gpu::image_access_mask::copy_destination,         u8"copy_destination"         ),
		std::pair(gpu::image_access_mask::color_render_target,      u8"color_render_target"      ),
		std::pair(gpu::image_access_mask::depth_stencil_read_only,  u8"depth_stencil_read_only"  ),
		std::pair(gpu::image_access_mask::depth_stencil_read_write, u8"depth_stencil_read_write" ),
		std::pair(gpu::image_access_mask::shader_read,              u8"shader_read"              ),
		std::pair(gpu::image_access_mask::shader_write,             u8"shader_write"             ),
	};


	const mapping<gpu::buffer_access_mask, std::u8string_view> name_mapping<gpu::buffer_access_mask>::value{
		std::pair(gpu::buffer_access_mask::copy_source,                        u8"copy_source"                       ),
		std::pair(gpu::buffer_access_mask::copy_destination,                   u8"copy_destination"                  ),
		std::pair(gpu::buffer_access_mask::vertex_buffer,                      u8"vertex_buffer"                     ),
		std::pair(gpu::buffer_access_mask::index_buffer,                       u8"index_buffer"                      ),
		std::pair(gpu::buffer_access_mask::constant_buffer,                    u8"constant_buffer"                   ),
		std::pair(gpu::buffer_access_mask::shader_read,                        u8"shader_read"                       ),
		std::pair(gpu::buffer_access_mask::shader_write,                       u8"shader_write"                      ),
		std::pair(gpu::buffer_access_mask::acceleration_structure_build_input, u8"acceleration_structure_build_input"),
		std::pair(gpu::buffer_access_mask::acceleration_structure_read,        u8"acceleration_structure_read"       ),
		std::pair(gpu::buffer_access_mask::acceleration_structure_write,       u8"acceleration_structure_write"      ),
		std::pair(gpu::buffer_access_mask::cpu_read,                           u8"cpu_read"                          ),
		std::pair(gpu::buffer_access_mask::cpu_write,                          u8"cpu_write"                         ),
	};
}
