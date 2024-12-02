#include "lotus/renderer/loaders/dds_loader.h"

/// \file
/// Implementation of the DDS loader.

#include "lotus/logging.h"
#include "lotus/gpu/common.h"
#include "lotus/gpu/backends/common/dxgi_format.h"

namespace lotus::dds {
	gpu::format loader::four_cc_to_format(std::uint32_t four_cc) {
		switch (four_cc) {
		case 36:  return gpu::format::r16g16b16a16_unorm;
		case 110: return gpu::format::r16g16b16a16_snorm;
		case 111: return gpu::format::r16_float;
		case 112: return gpu::format::r16g16_float;
		case 113: return gpu::format::r16g16b16a16_float;
		case 114: return gpu::format::r32_float;
		case 115: return gpu::format::r32g32_float;
		case 116: return gpu::format::r32g32b32a32_float;
		case 117: return gpu::format::none; // TODO D3DFMT_CxV8U8?
		case make_four_character_code(u8"DXT1"): return gpu::format::bc1_unorm;
		case make_four_character_code(u8"DXT3"): return gpu::format::bc2_unorm;
		case make_four_character_code(u8"DXT5"): return gpu::format::bc3_unorm;
		case make_four_character_code(u8"BC4U"): return gpu::format::bc4_unorm;
		case make_four_character_code(u8"BC4S"): return gpu::format::bc4_snorm;
		case make_four_character_code(u8"ATI2"): return gpu::format::bc5_unorm;
		case make_four_character_code(u8"BC5S"): return gpu::format::bc5_snorm;
		case make_four_character_code(u8"RGBG"): return gpu::format::none; // TODO D3DFMT_R8G8_B8G8 / DXGI_FORMAT_R8G8_B8G8_UNORM?
		case make_four_character_code(u8"GRGB"): return gpu::format::none; // TODO D3DFMT_G8R8_G8B8 / DXGI_FORMAT_G8R8_G8B8_UNORM?
		case make_four_character_code(u8"DXT2"): return gpu::format::none; // TODO D3DFMT_DXT2
		case make_four_character_code(u8"DXT4"): return gpu::format::none; // TODO D3DFMT_DXT4
		case make_four_character_code(u8"UYVY"): return gpu::format::none; // TODO D3DFMT_UYVY
		case make_four_character_code(u8"YUV2"): return gpu::format::none; // TODO D3DFMT_YUV2
		default: return gpu::format::none; // unknown
		}
	}

	// https://github.com/microsoft/DirectXTex/blob/main/DDSTextureLoader/DDSTextureLoader12.cpp, GetDXGIFormat()
	gpu::format loader::infer_format_from(const pixel_format &fmt) {
		if (bit_mask::contains<pixel_format_flags::rgb>(fmt.flags)) {
			switch (fmt.rgb_bit_count) {
			case 32:
				if (fmt.is_bit_mask(0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000)) {
					return gpu::format::r8g8b8a8_unorm;
				}
				if (fmt.is_bit_mask(0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)) {
					return gpu::format::b8g8r8a8_unorm;
				}
				if (fmt.is_bit_mask(0x0000FFFF, 0xFFFF0000, 0, 0)) {
					return gpu::format::r16g16_unorm;
				}
				if (fmt.is_bit_mask(0xFFFFFFFF, 0, 0, 0)) {
					return gpu::format::r32_float;
				}
				// TODO
				break;

			case 16:
				if (fmt.is_bit_mask(0x00FF, 0, 0, 0xFF00)) {
					return gpu::format::r8g8_unorm;
				}
				if (fmt.is_bit_mask(0xFFFF, 0, 0, 0)) {
					return gpu::format::r16_unorm;
				}
				// TODO
				break;

			case 8:
				if (fmt.is_bit_mask(0xFF, 0, 0, 0)) {
					return gpu::format::r8_unorm;
				}
				break;
			}
		} else if (bit_mask::contains<pixel_format_flags::luminance>(fmt.flags)) {
			switch (fmt.rgb_bit_count) {
			case 16:
				if (fmt.is_bit_mask(0xFFFF, 0, 0, 0)) {
					return gpu::format::r16_unorm;
				}
				if (fmt.is_bit_mask(0x00FF, 0, 0, 0xFF00)) {
					return gpu::format::r8g8_unorm;
				}
				break;

			case 8:
				if (fmt.is_bit_mask(0xFF, 0, 0, 0)) {
					return gpu::format::r8_unorm;
				}
				if (fmt.is_bit_mask(0x00FF, 0, 0, 0xFF00)) {
					return gpu::format::r8g8_unorm;
				}
				break;
			}
		} else if (bit_mask::contains<pixel_format_flags::alpha>(fmt.flags)) {
			if (fmt.rgb_bit_count == 8) {
				// TODO
			}
		} else if (bit_mask::contains<pixel_format_flags::bump_dudv>(fmt.flags)) {
			switch (fmt.rgb_bit_count) {
			case 32:
				if (fmt.is_bit_mask(0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000)) {
					return gpu::format::r8g8b8a8_snorm;
				}
				if (fmt.is_bit_mask(0x0000FFFF, 0xFFFF0000, 0, 0)) {
					return gpu::format::r16g16_snorm;
				}
				break;

			case 16:
				if (fmt.is_bit_mask(0x00FF, 0xFF00, 0, 0)) {
					return gpu::format::r8g8_snorm;
				}
				break;
			}
		} else if (bit_mask::contains<pixel_format_flags::four_cc>(fmt.flags)) {
			auto gpu_fmt = four_cc_to_format(fmt.four_cc);
			if (gpu_fmt == gpu::format::none) {
				log().error(
					"Unsupported four-character code: {:X}, '{}{}{}{}'",
					fmt.four_cc,
					static_cast<char>( fmt.four_cc        & 0xFF),
					static_cast<char>((fmt.four_cc >> 8)  & 0xFF),
					static_cast<char>((fmt.four_cc >> 16) & 0xFF),
					static_cast<char>((fmt.four_cc >> 24) & 0xFF)
				);
			}
			return gpu_fmt;
		}
		log().error(
			"Unsupported pixel format, flags: {:X}, bit count: {}, r: {:010X}, g: {:010X}, b: {:010X}, a: {:010X}",
			static_cast<std::underlying_type_t<pixel_format_flags>>(fmt.flags), fmt.rgb_bit_count,
			fmt.r_bit_mask, fmt.g_bit_mask, fmt.b_bit_mask, fmt.a_bit_mask
		);
		return gpu::format::none;
	}

	std::optional<loader> loader::create(std::span<const std::byte> data) {
		if (data.size() < sizeof(std::uint32_t) + sizeof(header)) {
			log().debug("DDS file too small");
			return std::nullopt;
		}

		loader result = nullptr;
		result._data = data.data();
		result._size = data.size();
		if (auto file_magic = *result._data_at_offset<std::uint32_t>(0); file_magic != magic) {
			log().debug("Incorrect DDS magic number: {}", file_magic);
			return std::nullopt;
		}

		const auto &dds_header = result.get_header();
		if (dds_header.size != sizeof(header)) {
			log().debug("Incorrect DDS header size: {}", dds_header.size);
			return std::nullopt;
		}
		if (dds_header.pixel_format.size != sizeof(pixel_format)) {
			log().debug("Incorrect DDS pixel format size: {}", dds_header.pixel_format.size);
			return std::nullopt;
		}

		result._width      = dds_header.width;
		result._height     = dds_header.height;
		result._depth      = dds_header.depth;
		result._array_size = 1;
		result._num_mips   = std::max<std::uint32_t>(1, dds_header.mipmap_count);

		result._has_dx10_header =
			bit_mask::contains<pixel_format_flags::four_cc>(dds_header.pixel_format.flags) &&
			dds_header.pixel_format.four_cc == make_four_character_code(u8"DX10");
		if (result._has_dx10_header) {
			if (result.get_dx10_header() == nullptr) {
				log().debug("DDS file too small");
				return std::nullopt;
			}

			const auto *dds_dx10_header = result.get_dx10_header();

			result._format = gpu::backends::common::_details::conversions::back_to_format(
				static_cast<DXGI_FORMAT>(dds_dx10_header->dxgi_format)
			);
			if (result._format == gpu::format::none) {
				log().debug("Unsupported format: {}", dds_dx10_header->dxgi_format);
				return std::nullopt;
			}

			result._array_size = dds_dx10_header->array_size;
			if (result._array_size == 0) {
				log().debug("Zero array size not supported");
				return std::nullopt;
			}

			switch (dds_dx10_header->dimension) {
			case resource_dimension::texture1d:
				if (bit_mask::contains<header_flags::height>(dds_header.flags) && dds_header.height != 1) {
					log().debug("1D texture with invalid height value: {}", dds_header.height);
					return std::nullopt;
				}
				result._height = 1;
				result._depth  = 1;
				break;
			case resource_dimension::texture2d:
				if (bit_mask::contains<miscellaneous_flags::texture_cube>(dds_dx10_header->flags)) {
					result._array_size *= 6;
					result._is_cubemap = true;
				}
				result._depth = 1;
				break;
			case resource_dimension::texture3d:
				if (!bit_mask::contains<header_flags::depth>(dds_header.flags)) {
					log().debug("3D texture without depth flag");
					return std::nullopt;
				}
				if (result._array_size > 1) {
					log().debug("Array of 3D textures is not supported");
					return std::nullopt;
				}
				break;
			case resource_dimension::buffer:
				log().debug("DDS: Buffers are not supported");
				return std::nullopt;
			case resource_dimension::unknown:
				log().debug("DDS: Unknown resource dimension");
				return std::nullopt;
			}
		} else {
			result._format = infer_format_from(dds_header.pixel_format);
			if (result._format == gpu::format::none) {
				log().debug("Cannot deduce pixel format");
				return std::nullopt;
			}

			if (!bit_mask::contains<header_flags::depth>(dds_header.flags)) {
				if (bit_mask::contains<capabilities2::cubemap>(dds_header.caps2)) {
					if (
						(dds_header.caps2 & capabilities2::cubemap_all_faces) !=
						capabilities2::cubemap_all_faces
					) {
						log().debug("Missing cubemap faces");
						return std::nullopt;
					}

					result._array_size = 6;
					result._is_cubemap = true;
				}
				result._depth = 1;
			}
		}

		return result;
	}
}
