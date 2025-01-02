#include "lotus/gpu/backends/metal/details.h"

/// \file
/// Metal backend utilities.

namespace lotus::gpu::backends::metal::_details {
	namespace conversions {
		MTL::PixelFormat to_pixel_format(format f) {
			constexpr static enums::sequential_mapping<format, MTL::PixelFormat> table{
				std::pair(format::none,               MTL::PixelFormatInvalid              ),
				std::pair(format::d32_float_s8,       MTL::PixelFormatDepth32Float_Stencil8),
				std::pair(format::d32_float,          MTL::PixelFormatDepth32Float         ),
				std::pair(format::d24_unorm_s8,       MTL::PixelFormatDepth24Unorm_Stencil8),
				std::pair(format::d16_unorm,          MTL::PixelFormatDepth16Unorm         ),
				std::pair(format::r8_unorm,           MTL::PixelFormatR8Unorm              ),
				std::pair(format::r8_snorm,           MTL::PixelFormatR8Snorm              ),
				std::pair(format::r8_uint,            MTL::PixelFormatR8Uint               ),
				std::pair(format::r8_sint,            MTL::PixelFormatR8Sint               ),
				std::pair(format::r8g8_unorm,         MTL::PixelFormatRG8Unorm             ),
				std::pair(format::r8g8_snorm,         MTL::PixelFormatRG8Snorm             ),
				std::pair(format::r8g8_uint,          MTL::PixelFormatRG8Uint              ),
				std::pair(format::r8g8_sint,          MTL::PixelFormatRG8Sint              ),
				std::pair(format::r8g8b8a8_unorm,     MTL::PixelFormatRGBA8Unorm           ),
				std::pair(format::r8g8b8a8_snorm,     MTL::PixelFormatRGBA8Snorm           ),
				std::pair(format::r8g8b8a8_srgb,      MTL::PixelFormatRGBA8Unorm_sRGB      ),
				std::pair(format::r8g8b8a8_uint,      MTL::PixelFormatRGBA8Uint            ),
				std::pair(format::r8g8b8a8_sint,      MTL::PixelFormatRGBA8Sint            ),
				std::pair(format::b8g8r8a8_unorm,     MTL::PixelFormatBGRA8Unorm           ),
				std::pair(format::b8g8r8a8_srgb,      MTL::PixelFormatBGRA8Unorm_sRGB      ),
				std::pair(format::r16_unorm,          MTL::PixelFormatR16Unorm             ),
				std::pair(format::r16_snorm,          MTL::PixelFormatR16Snorm             ),
				std::pair(format::r16_uint,           MTL::PixelFormatR16Uint              ),
				std::pair(format::r16_sint,           MTL::PixelFormatR16Sint              ),
				std::pair(format::r16_float,          MTL::PixelFormatR16Float             ),
				std::pair(format::r16g16_unorm,       MTL::PixelFormatRG16Unorm            ),
				std::pair(format::r16g16_snorm,       MTL::PixelFormatRG16Snorm            ),
				std::pair(format::r16g16_uint,        MTL::PixelFormatRG16Uint             ),
				std::pair(format::r16g16_sint,        MTL::PixelFormatRG16Sint             ),
				std::pair(format::r16g16_float,       MTL::PixelFormatRG16Float            ),
				std::pair(format::r16g16b16a16_unorm, MTL::PixelFormatRGBA16Unorm          ),
				std::pair(format::r16g16b16a16_snorm, MTL::PixelFormatRGBA16Snorm          ),
				std::pair(format::r16g16b16a16_uint,  MTL::PixelFormatRGBA16Uint           ),
				std::pair(format::r16g16b16a16_sint,  MTL::PixelFormatRGBA16Sint           ),
				std::pair(format::r16g16b16a16_float, MTL::PixelFormatRGBA16Float          ),
				std::pair(format::r32_uint,           MTL::PixelFormatR32Uint              ),
				std::pair(format::r32_sint,           MTL::PixelFormatR32Sint              ),
				std::pair(format::r32_float,          MTL::PixelFormatR32Float             ),
				std::pair(format::r32g32_uint,        MTL::PixelFormatRG32Uint             ),
				std::pair(format::r32g32_sint,        MTL::PixelFormatRG32Sint             ),
				std::pair(format::r32g32_float,       MTL::PixelFormatRG32Float            ),
				std::pair(format::r32g32b32_uint,     MTL::PixelFormatInvalid              ), // not supported!
				std::pair(format::r32g32b32_sint,     MTL::PixelFormatInvalid              ), // not supported!
				std::pair(format::r32g32b32_float,    MTL::PixelFormatInvalid              ), // not supported!
				std::pair(format::r32g32b32a32_uint,  MTL::PixelFormatRGBA32Uint           ),
				std::pair(format::r32g32b32a32_sint,  MTL::PixelFormatRGBA32Sint           ),
				std::pair(format::r32g32b32a32_float, MTL::PixelFormatRGBA32Float          ),
				std::pair(format::bc1_unorm,          MTL::PixelFormatBC1_RGBA             ),
				std::pair(format::bc1_srgb,           MTL::PixelFormatBC1_RGBA_sRGB        ),
				std::pair(format::bc2_unorm,          MTL::PixelFormatBC2_RGBA             ),
				std::pair(format::bc2_srgb,           MTL::PixelFormatBC2_RGBA_sRGB        ),
				std::pair(format::bc3_unorm,          MTL::PixelFormatBC3_RGBA             ),
				std::pair(format::bc3_srgb,           MTL::PixelFormatBC3_RGBA_sRGB        ),
				std::pair(format::bc4_unorm,          MTL::PixelFormatBC4_RUnorm           ),
				std::pair(format::bc4_snorm,          MTL::PixelFormatBC4_RSnorm           ),
				std::pair(format::bc5_unorm,          MTL::PixelFormatBC5_RGUnorm          ),
				std::pair(format::bc5_snorm,          MTL::PixelFormatBC5_RGSnorm          ),
				std::pair(format::bc6h_f16,           MTL::PixelFormatBC6H_RGBFloat        ),
				std::pair(format::bc6h_uf16,          MTL::PixelFormatBC6H_RGBUfloat       ),
				std::pair(format::bc7_unorm,          MTL::PixelFormatBC7_RGBAUnorm        ),
				std::pair(format::bc7_srgb,           MTL::PixelFormatBC7_RGBAUnorm_sRGB   ),
			};
			return table[f];
		}

		MTL::ResourceOptions to_resource_options(_details::memory_type_index i) {
			constexpr static enums::sequential_mapping<memory_type_index, MTL::ResourceOptions> table{
				std::pair(memory_type_index::shared_cpu_cached,   MTL::ResourceStorageModeShared | MTL::ResourceOptionCPUCacheModeDefault      ),
				std::pair(memory_type_index::shared_cpu_uncached, MTL::ResourceStorageModeShared | MTL::ResourceOptionCPUCacheModeWriteCombined),
				std::pair(memory_type_index::device_private,      MTL::ResourceStorageModePrivate                                              ),
			};
			return table[i];
		}

		NS::Range to_range(mip_levels mips, MTL::Texture *tex) {
			return NS::Range(
				mips.first_level,
				mips.is_tail() ? tex->mipmapLevelCount() - mips.first_level : mips.num_levels
			);
		}

		MTL::TextureUsage to_texture_usage(image_usage_mask mask) {
			constexpr static bit_mask::mapping<image_usage_mask, MTL::TextureUsage> table{
				std::pair(image_usage_mask::copy_source,                 MTL::TextureUsageShaderRead  ),
				std::pair(image_usage_mask::copy_destination,            MTL::TextureUsageShaderWrite ),
				std::pair(image_usage_mask::shader_read,                 MTL::TextureUsageShaderRead  ),
				std::pair(image_usage_mask::shader_write,                MTL::TextureUsageShaderWrite ),
				std::pair(image_usage_mask::color_render_target,         MTL::TextureUsageRenderTarget),
				std::pair(image_usage_mask::depth_stencil_render_target, MTL::TextureUsageRenderTarget),
			};
			return table.get_union(mask);
		}

		MTL::SamplerAddressMode to_sampler_address_mode(sampler_address_mode mode) {
			constexpr static enums::sequential_mapping<sampler_address_mode, MTL::SamplerAddressMode> table{
				std::pair(sampler_address_mode::repeat, MTL::SamplerAddressModeRepeat            ),
				std::pair(sampler_address_mode::mirror, MTL::SamplerAddressModeMirrorRepeat      ),
				std::pair(sampler_address_mode::clamp,  MTL::SamplerAddressModeClampToEdge       ),
				std::pair(sampler_address_mode::border, MTL::SamplerAddressModeClampToBorderColor),
			};
			return table[mode];
		}

		MTL::SamplerMinMagFilter to_sampler_min_mag_filter(filtering f) {
			constexpr static enums::sequential_mapping<filtering, MTL::SamplerMinMagFilter> table{
				std::pair(filtering::nearest, MTL::SamplerMinMagFilterNearest),
				std::pair(filtering::linear,  MTL::SamplerMinMagFilterLinear ),
			};
			return table[f];
		}

		MTL::SamplerMipFilter to_sampler_mip_filter(filtering f) {
			constexpr static enums::sequential_mapping<filtering, MTL::SamplerMipFilter> table{
				std::pair(filtering::nearest, MTL::SamplerMipFilterNearest),
				std::pair(filtering::linear,  MTL::SamplerMipFilterLinear ),
			};
			return table[f];
		}

		metal_ptr<NS::String> to_string(const char8_t *str) {
			return take_ownership(NS::String::string(reinterpret_cast<const char*>(str), NS::UTF8StringEncoding));
		}


		std::u8string back_to_string(NS::String *str) {
			return std::u8string(
				reinterpret_cast<const char8_t*>(str->utf8String()),
				static_cast<std::size_t>(str->lengthOfBytes(NS::UTF8StringEncoding))
			);
		}

		memory::size_alignment back_to_size_alignment(MTL::SizeAndAlign sa) {
			return memory::size_alignment(sa.size, sa.align);
		}
	}

	_details::metal_ptr<MTL::TextureDescriptor> create_texture_descriptor(
		MTL::TextureType type,
		format fmt,
		cvec3u32 size,
		std::uint32_t mip_levels,
		MTL::ResourceOptions opts,
		image_usage_mask usages
	) {
		auto descriptor = take_ownership(MTL::TextureDescriptor::alloc()->init());
		descriptor->setTextureType(type);
		descriptor->setPixelFormat(conversions::to_pixel_format(fmt));
		descriptor->setWidth(size[0]);
		descriptor->setHeight(size[1]);
		descriptor->setDepth(size[2]);
		descriptor->setMipmapLevelCount(mip_levels);
		descriptor->setResourceOptions(opts);
		descriptor->setUsage(conversions::to_texture_usage(usages));
		return descriptor;
	}
}
