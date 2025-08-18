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

		MTL::VertexFormat to_vertex_format(format f) {
			constexpr static enums::sequential_mapping<format, MTL::VertexFormat> table{
				std::pair(format::none,               MTL::VertexFormatInvalid              ),
				std::pair(format::d32_float_s8,       MTL::VertexFormatInvalid              ),
				std::pair(format::d32_float,          MTL::VertexFormatInvalid              ),
				std::pair(format::d24_unorm_s8,       MTL::VertexFormatInvalid              ),
				std::pair(format::d16_unorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::r8_unorm,           MTL::VertexFormatUCharNormalized      ),
				std::pair(format::r8_snorm,           MTL::VertexFormatCharNormalized       ),
				std::pair(format::r8_uint,            MTL::VertexFormatUChar                ),
				std::pair(format::r8_sint,            MTL::VertexFormatChar                 ),
				std::pair(format::r8g8_unorm,         MTL::VertexFormatUChar2Normalized     ),
				std::pair(format::r8g8_snorm,         MTL::VertexFormatChar2Normalized      ),
				std::pair(format::r8g8_uint,          MTL::VertexFormatUChar2               ),
				std::pair(format::r8g8_sint,          MTL::VertexFormatChar2                ),
				std::pair(format::r8g8b8a8_unorm,     MTL::VertexFormatUChar4Normalized     ),
				std::pair(format::r8g8b8a8_snorm,     MTL::VertexFormatChar4Normalized      ),
				std::pair(format::r8g8b8a8_srgb,      MTL::VertexFormatInvalid              ),
				std::pair(format::r8g8b8a8_uint,      MTL::VertexFormatUChar4               ),
				std::pair(format::r8g8b8a8_sint,      MTL::VertexFormatChar4                ),
				std::pair(format::b8g8r8a8_unorm,     MTL::VertexFormatUChar4Normalized_BGRA),
				std::pair(format::b8g8r8a8_srgb,      MTL::VertexFormatInvalid              ),
				std::pair(format::r16_unorm,          MTL::VertexFormatUShortNormalized     ),
				std::pair(format::r16_snorm,          MTL::VertexFormatShortNormalized      ),
				std::pair(format::r16_uint,           MTL::VertexFormatUShort               ),
				std::pair(format::r16_sint,           MTL::VertexFormatShort                ),
				std::pair(format::r16_float,          MTL::VertexFormatHalf                 ),
				std::pair(format::r16g16_unorm,       MTL::VertexFormatUShort2Normalized    ),
				std::pair(format::r16g16_snorm,       MTL::VertexFormatShort2Normalized     ),
				std::pair(format::r16g16_uint,        MTL::VertexFormatUShort2              ),
				std::pair(format::r16g16_sint,        MTL::VertexFormatShort2               ),
				std::pair(format::r16g16_float,       MTL::VertexFormatHalf2                ),
				std::pair(format::r16g16b16a16_unorm, MTL::VertexFormatUShort4Normalized    ),
				std::pair(format::r16g16b16a16_snorm, MTL::VertexFormatShort4Normalized     ),
				std::pair(format::r16g16b16a16_uint,  MTL::VertexFormatUShort4              ),
				std::pair(format::r16g16b16a16_sint,  MTL::VertexFormatShort4               ),
				std::pair(format::r16g16b16a16_float, MTL::VertexFormatHalf4                ),
				std::pair(format::r32_uint,           MTL::VertexFormatUInt                 ),
				std::pair(format::r32_sint,           MTL::VertexFormatInt                  ),
				std::pair(format::r32_float,          MTL::VertexFormatFloat                ),
				std::pair(format::r32g32_uint,        MTL::VertexFormatUInt2                ),
				std::pair(format::r32g32_sint,        MTL::VertexFormatInt2                 ),
				std::pair(format::r32g32_float,       MTL::VertexFormatFloat2               ),
				std::pair(format::r32g32b32_uint,     MTL::VertexFormatUInt3                ),
				std::pair(format::r32g32b32_sint,     MTL::VertexFormatInt3                 ),
				std::pair(format::r32g32b32_float,    MTL::VertexFormatFloat3               ),
				std::pair(format::r32g32b32a32_uint,  MTL::VertexFormatUInt4                ),
				std::pair(format::r32g32b32a32_sint,  MTL::VertexFormatInt4                 ),
				std::pair(format::r32g32b32a32_float, MTL::VertexFormatFloat4               ),
				std::pair(format::bc1_unorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc1_srgb,           MTL::VertexFormatInvalid              ),
				std::pair(format::bc2_unorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc2_srgb,           MTL::VertexFormatInvalid              ),
				std::pair(format::bc3_unorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc3_srgb,           MTL::VertexFormatInvalid              ),
				std::pair(format::bc4_unorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc4_snorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc5_unorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc5_snorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc6h_f16,           MTL::VertexFormatInvalid              ),
				std::pair(format::bc6h_uf16,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc7_unorm,          MTL::VertexFormatInvalid              ),
				std::pair(format::bc7_srgb,           MTL::VertexFormatInvalid              ),
			};
			return table[f];
		}

		MTL::AttributeFormat to_attribute_format(format f) {
			constexpr static enums::sequential_mapping<format, MTL::AttributeFormat> table{
				std::pair(format::none,               MTL::AttributeFormatInvalid              ),
				std::pair(format::d32_float_s8,       MTL::AttributeFormatInvalid              ),
				std::pair(format::d32_float,          MTL::AttributeFormatInvalid              ),
				std::pair(format::d24_unorm_s8,       MTL::AttributeFormatInvalid              ),
				std::pair(format::d16_unorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::r8_unorm,           MTL::AttributeFormatUCharNormalized      ),
				std::pair(format::r8_snorm,           MTL::AttributeFormatCharNormalized       ),
				std::pair(format::r8_uint,            MTL::AttributeFormatUChar                ),
				std::pair(format::r8_sint,            MTL::AttributeFormatChar                 ),
				std::pair(format::r8g8_unorm,         MTL::AttributeFormatUChar2Normalized     ),
				std::pair(format::r8g8_snorm,         MTL::AttributeFormatChar2Normalized      ),
				std::pair(format::r8g8_uint,          MTL::AttributeFormatUChar2               ),
				std::pair(format::r8g8_sint,          MTL::AttributeFormatChar2                ),
				std::pair(format::r8g8b8a8_unorm,     MTL::AttributeFormatUChar4Normalized     ),
				std::pair(format::r8g8b8a8_snorm,     MTL::AttributeFormatChar4Normalized      ),
				std::pair(format::r8g8b8a8_srgb,      MTL::AttributeFormatInvalid              ),
				std::pair(format::r8g8b8a8_uint,      MTL::AttributeFormatUChar4               ),
				std::pair(format::r8g8b8a8_sint,      MTL::AttributeFormatChar4                ),
				std::pair(format::b8g8r8a8_unorm,     MTL::AttributeFormatUChar4Normalized_BGRA),
				std::pair(format::b8g8r8a8_srgb,      MTL::AttributeFormatInvalid              ),
				std::pair(format::r16_unorm,          MTL::AttributeFormatUShortNormalized     ),
				std::pair(format::r16_snorm,          MTL::AttributeFormatShortNormalized      ),
				std::pair(format::r16_uint,           MTL::AttributeFormatUShort               ),
				std::pair(format::r16_sint,           MTL::AttributeFormatShort                ),
				std::pair(format::r16_float,          MTL::AttributeFormatHalf                 ),
				std::pair(format::r16g16_unorm,       MTL::AttributeFormatUShort2Normalized    ),
				std::pair(format::r16g16_snorm,       MTL::AttributeFormatShort2Normalized     ),
				std::pair(format::r16g16_uint,        MTL::AttributeFormatUShort2              ),
				std::pair(format::r16g16_sint,        MTL::AttributeFormatShort2               ),
				std::pair(format::r16g16_float,       MTL::AttributeFormatHalf2                ),
				std::pair(format::r16g16b16a16_unorm, MTL::AttributeFormatUShort4Normalized    ),
				std::pair(format::r16g16b16a16_snorm, MTL::AttributeFormatShort4Normalized     ),
				std::pair(format::r16g16b16a16_uint,  MTL::AttributeFormatUShort4              ),
				std::pair(format::r16g16b16a16_sint,  MTL::AttributeFormatShort4               ),
				std::pair(format::r16g16b16a16_float, MTL::AttributeFormatHalf4                ),
				std::pair(format::r32_uint,           MTL::AttributeFormatUInt                 ),
				std::pair(format::r32_sint,           MTL::AttributeFormatInt                  ),
				std::pair(format::r32_float,          MTL::AttributeFormatFloat                ),
				std::pair(format::r32g32_uint,        MTL::AttributeFormatUInt2                ),
				std::pair(format::r32g32_sint,        MTL::AttributeFormatInt2                 ),
				std::pair(format::r32g32_float,       MTL::AttributeFormatFloat2               ),
				std::pair(format::r32g32b32_uint,     MTL::AttributeFormatUInt3                ),
				std::pair(format::r32g32b32_sint,     MTL::AttributeFormatInt3                 ),
				std::pair(format::r32g32b32_float,    MTL::AttributeFormatFloat3               ),
				std::pair(format::r32g32b32a32_uint,  MTL::AttributeFormatUInt4                ),
				std::pair(format::r32g32b32a32_sint,  MTL::AttributeFormatInt4                 ),
				std::pair(format::r32g32b32a32_float, MTL::AttributeFormatFloat4               ),
				std::pair(format::bc1_unorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc1_srgb,           MTL::AttributeFormatInvalid              ),
				std::pair(format::bc2_unorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc2_srgb,           MTL::AttributeFormatInvalid              ),
				std::pair(format::bc3_unorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc3_srgb,           MTL::AttributeFormatInvalid              ),
				std::pair(format::bc4_unorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc4_snorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc5_unorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc5_snorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc6h_f16,           MTL::AttributeFormatInvalid              ),
				std::pair(format::bc6h_uf16,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc7_unorm,          MTL::AttributeFormatInvalid              ),
				std::pair(format::bc7_srgb,           MTL::AttributeFormatInvalid              ),
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

		MTL::CompareFunction to_compare_function(comparison_function cmp) {
			constexpr static enums::sequential_mapping<comparison_function, MTL::CompareFunction> table{
				std::pair(comparison_function::none,             MTL::CompareFunctionNever       ),
				std::pair(comparison_function::never,            MTL::CompareFunctionNever       ),
				std::pair(comparison_function::less,             MTL::CompareFunctionLess        ),
				std::pair(comparison_function::equal,            MTL::CompareFunctionEqual       ),
				std::pair(comparison_function::less_or_equal,    MTL::CompareFunctionLessEqual   ),
				std::pair(comparison_function::greater,          MTL::CompareFunctionGreater     ),
				std::pair(comparison_function::not_equal,        MTL::CompareFunctionNotEqual    ),
				std::pair(comparison_function::greater_or_equal, MTL::CompareFunctionGreaterEqual),
				std::pair(comparison_function::always,           MTL::CompareFunctionAlways      ),
			};
			return table[cmp];
		}

		MTL::LoadAction to_load_action(pass_load_operation o) {
			constexpr static enums::sequential_mapping<pass_load_operation, MTL::LoadAction> table{
				std::pair(pass_load_operation::discard,  MTL::LoadActionDontCare),
				std::pair(pass_load_operation::preserve, MTL::LoadActionLoad    ),
				std::pair(pass_load_operation::clear,    MTL::LoadActionClear   ),
			};
			return table[o];
		}

		MTL::StoreAction to_store_action(pass_store_operation o) {
			constexpr static enums::sequential_mapping<pass_store_operation, MTL::StoreAction> table{
				std::pair(pass_store_operation::discard,  MTL::StoreActionDontCare),
				std::pair(pass_store_operation::preserve, MTL::StoreActionStore   ),
			};
			return table[o];
		}

		MTL::PrimitiveType to_primitive_type(primitive_topology t) {
			constexpr static enums::sequential_mapping<primitive_topology, MTL::PrimitiveType> table{
				std::pair(primitive_topology::point_list,                    MTL::PrimitiveTypePoint        ),
				std::pair(primitive_topology::line_list,                     MTL::PrimitiveTypeLine         ),
				std::pair(primitive_topology::line_strip,                    MTL::PrimitiveTypeLineStrip    ),
				std::pair(primitive_topology::triangle_list,                 MTL::PrimitiveTypeTriangle     ),
				std::pair(primitive_topology::triangle_strip,                MTL::PrimitiveTypeTriangleStrip),
				// TODO what to do about these?
				std::pair(primitive_topology::line_list_with_adjacency,      MTL::PrimitiveTypeLine         ),
				std::pair(primitive_topology::line_strip_with_adjacency,     MTL::PrimitiveTypeLineStrip    ),
				std::pair(primitive_topology::triangle_list_with_adjacency,  MTL::PrimitiveTypeTriangle     ),
				std::pair(primitive_topology::triangle_strip_with_adjacency, MTL::PrimitiveTypeTriangleStrip),
			};
			return table[t];
		}

		MTL::PrimitiveTopologyClass to_primitive_topology_class(primitive_topology t) {
			constexpr static enums::sequential_mapping<primitive_topology, MTL::PrimitiveTopologyClass> table{
				std::pair(primitive_topology::point_list,                    MTL::PrimitiveTopologyClassPoint   ),
				std::pair(primitive_topology::line_list,                     MTL::PrimitiveTopologyClassLine    ),
				std::pair(primitive_topology::line_strip,                    MTL::PrimitiveTopologyClassLine    ),
				std::pair(primitive_topology::triangle_list,                 MTL::PrimitiveTopologyClassTriangle),
				std::pair(primitive_topology::triangle_strip,                MTL::PrimitiveTopologyClassTriangle),
				std::pair(primitive_topology::line_list_with_adjacency,      MTL::PrimitiveTopologyClassLine    ),
				std::pair(primitive_topology::line_strip_with_adjacency,     MTL::PrimitiveTopologyClassLine    ),
				std::pair(primitive_topology::triangle_list_with_adjacency,  MTL::PrimitiveTopologyClassTriangle),
				std::pair(primitive_topology::triangle_strip_with_adjacency, MTL::PrimitiveTopologyClassTriangle),
			};
			return table[t];
		}

		MTL::IndexType to_index_type(index_format f) {
			constexpr static enums::sequential_mapping<index_format, MTL::IndexType> table{
				std::pair(index_format::uint16, MTL::IndexTypeUInt16),
				std::pair(index_format::uint32, MTL::IndexTypeUInt32),
			};
			return table[f];
		}

		MTL::VertexStepFunction to_vertex_step_function(input_buffer_rate r) {
			constexpr static enums::sequential_mapping<input_buffer_rate, MTL::VertexStepFunction> table{
				std::pair(input_buffer_rate::per_vertex,   MTL::VertexStepFunctionPerVertex  ),
				std::pair(input_buffer_rate::per_instance, MTL::VertexStepFunctionPerInstance),
			};
			return table[r];
		}

		MTL::Winding to_winding(front_facing_mode f) {
			constexpr static enums::sequential_mapping<front_facing_mode, MTL::Winding> table{
				std::pair(front_facing_mode::clockwise,         MTL::WindingClockwise       ),
				std::pair(front_facing_mode::counter_clockwise, MTL::WindingCounterClockwise),
			};
			return table[f];
		}

		MTL::CullMode to_cull_mode(cull_mode c) {
			constexpr static enums::sequential_mapping<cull_mode, MTL::CullMode> table{
				std::pair(cull_mode::none,       MTL::CullModeNone ),
				std::pair(cull_mode::cull_front, MTL::CullModeFront),
				std::pair(cull_mode::cull_back,  MTL::CullModeBack ),
			};
			return table[c];
		}

		MTL::StencilOperation to_stencil_operation(stencil_operation s) {
			constexpr static enums::sequential_mapping<stencil_operation, MTL::StencilOperation> table{
				std::pair(stencil_operation::keep,                MTL::StencilOperationKeep          ),
				std::pair(stencil_operation::zero,                MTL::StencilOperationZero          ),
				std::pair(stencil_operation::replace,             MTL::StencilOperationReplace       ),
				std::pair(stencil_operation::increment_and_clamp, MTL::StencilOperationIncrementClamp),
				std::pair(stencil_operation::decrement_and_clamp, MTL::StencilOperationDecrementClamp),
				std::pair(stencil_operation::bitwise_invert,      MTL::StencilOperationInvert        ),
				std::pair(stencil_operation::increment_and_wrap,  MTL::StencilOperationIncrementWrap ),
				std::pair(stencil_operation::decrement_and_wrap,  MTL::StencilOperationDecrementWrap ),
			};
			return table[s];
		}

		MTL::BlendOperation to_blend_operation(blend_operation b) {
			constexpr static enums::sequential_mapping<blend_operation, MTL::BlendOperation> table{
				std::pair(blend_operation::add,              MTL::BlendOperationAdd            ),
				std::pair(blend_operation::subtract,         MTL::BlendOperationSubtract       ),
				std::pair(blend_operation::reverse_subtract, MTL::BlendOperationReverseSubtract),
				std::pair(blend_operation::min,              MTL::BlendOperationMin            ),
				std::pair(blend_operation::max,              MTL::BlendOperationMax            ),
			};
			return table[b];
		}

		MTL::BlendFactor to_blend_factor(blend_factor b) {
			constexpr static enums::sequential_mapping<blend_factor, MTL::BlendFactor> table{
				std::pair(blend_factor::zero,                        MTL::BlendFactorZero                    ),
				std::pair(blend_factor::one,                         MTL::BlendFactorOne                     ),
				std::pair(blend_factor::source_color,                MTL::BlendFactorSourceColor             ),
				std::pair(blend_factor::one_minus_source_color,      MTL::BlendFactorOneMinusSourceColor     ),
				std::pair(blend_factor::destination_color,           MTL::BlendFactorDestinationColor        ),
				std::pair(blend_factor::one_minus_destination_color, MTL::BlendFactorOneMinusDestinationColor),
				std::pair(blend_factor::source_alpha,                MTL::BlendFactorSourceAlpha             ),
				std::pair(blend_factor::one_minus_source_alpha,      MTL::BlendFactorOneMinusSourceAlpha     ),
				std::pair(blend_factor::destination_alpha,           MTL::BlendFactorDestinationAlpha        ),
				std::pair(blend_factor::one_minus_destination_alpha, MTL::BlendFactorOneMinusDestinationAlpha),
			};
			return table[b];
		}

		MTL::ColorWriteMask to_color_write_mask(channel_mask m) {
			constexpr static bit_mask::mapping<channel_mask, MTL::ColorWriteMask> table{
				std::pair(channel_mask::red,   MTL::ColorWriteMaskRed  ),
				std::pair(channel_mask::green, MTL::ColorWriteMaskGreen),
				std::pair(channel_mask::blue,  MTL::ColorWriteMaskBlue ),
				std::pair(channel_mask::alpha, MTL::ColorWriteMaskAlpha),
			};
			return table.get_union(m);
		}

		MTL::ShaderValidation to_shader_validation(context_options opts) {
			return
				bit_mask::contains<context_options::enable_validation>(opts) ?
				MTL::ShaderValidationEnabled :
				MTL::ShaderValidationDisabled;
		}

		MTL::AccelerationStructureInstanceOptions to_acceleration_structure_instance_options(
			raytracing_instance_flags flags
		) {
			constexpr static bit_mask::mapping<raytracing_instance_flags, MTL::AccelerationStructureInstanceOptions> table{
				std::pair(raytracing_instance_flags::disable_triangle_culling,        MTL::AccelerationStructureInstanceOptionDisableTriangleCulling                    ),
				std::pair(raytracing_instance_flags::triangle_front_counterclockwise, MTL::AccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise),
				std::pair(raytracing_instance_flags::force_opaque,                    MTL::AccelerationStructureInstanceOptionOpaque                                    ),
				std::pair(raytracing_instance_flags::force_non_opaque,                MTL::AccelerationStructureInstanceOptionNonOpaque                                 ),
			};
			return table.get_union(flags);
		}

		IRDescriptorRangeType to_ir_descriptor_range_type(D3D_SHADER_INPUT_TYPE type) {
			constexpr static enums::sequential_mapping<
				D3D_SHADER_INPUT_TYPE, IRDescriptorRangeType, D3D_SIT_UAV_FEEDBACKTEXTURE + 1
			> table{
				std::pair(D3D_SIT_CBUFFER,                       IRDescriptorRangeTypeCBV    ),
				std::pair(D3D_SIT_TBUFFER,                       IRDescriptorRangeTypeSRV    ),
				std::pair(D3D_SIT_TEXTURE,                       IRDescriptorRangeTypeSRV    ),
				std::pair(D3D_SIT_SAMPLER,                       IRDescriptorRangeTypeSampler),
				std::pair(D3D_SIT_UAV_RWTYPED,                   IRDescriptorRangeTypeUAV    ),
				std::pair(D3D_SIT_STRUCTURED,                    IRDescriptorRangeTypeSRV    ),
				std::pair(D3D_SIT_UAV_RWSTRUCTURED,              IRDescriptorRangeTypeUAV    ),
				std::pair(D3D_SIT_BYTEADDRESS,                   IRDescriptorRangeTypeSRV    ),
				std::pair(D3D_SIT_UAV_RWBYTEADDRESS,             IRDescriptorRangeTypeUAV    ),
				std::pair(D3D_SIT_UAV_APPEND_STRUCTURED,         IRDescriptorRangeTypeUAV    ),
				std::pair(D3D_SIT_UAV_CONSUME_STRUCTURED,        IRDescriptorRangeTypeUAV    ),
				std::pair(D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER, IRDescriptorRangeTypeUAV    ),
				std::pair(D3D_SIT_RTACCELERATIONSTRUCTURE,       IRDescriptorRangeTypeSRV    ),
				std::pair(D3D_SIT_UAV_FEEDBACKTEXTURE,           IRDescriptorRangeTypeUAV    ),
			};
			return table[type];
		}

		IRShaderStage to_ir_shader_stage(shader_stage stage) {
			constexpr static enums::sequential_mapping<shader_stage, IRShaderStage> table{
				std::pair(shader_stage::all,                   IRShaderStageInvalid      ),
				std::pair(shader_stage::vertex_shader,         IRShaderStageVertex       ),
				std::pair(shader_stage::geometry_shader,       IRShaderStageGeometry     ),
				std::pair(shader_stage::pixel_shader,          IRShaderStageFragment     ),
				std::pair(shader_stage::compute_shader,        IRShaderStageCompute      ),
				std::pair(shader_stage::callable_shader,       IRShaderStageCallable     ),
				std::pair(shader_stage::ray_generation_shader, IRShaderStageRayGeneration),
				std::pair(shader_stage::intersection_shader,   IRShaderStageIntersection ),
				std::pair(shader_stage::any_hit_shader,        IRShaderStageAnyHit       ),
				std::pair(shader_stage::closest_hit_shader,    IRShaderStageClosestHit   ),
				std::pair(shader_stage::miss_shader,           IRShaderStageMiss         ),
			};
			return table[stage];
		}

		NS::SharedPtr<NS::String> to_string(const char8_t *str) {
			return NS::TransferPtr(NS::String::alloc()->init(
				reinterpret_cast<const char*>(str), NS::UTF8StringEncoding
			));
		}

		NS::SharedPtr<MTL::StencilDescriptor> to_stencil_descriptor(
			stencil_options s, u8 stencil_read, u8 stencil_write
		) {
			auto desc = NS::TransferPtr(MTL::StencilDescriptor::alloc()->init());
			desc->setStencilFailureOperation(to_stencil_operation(s.fail));
			desc->setDepthFailureOperation(to_stencil_operation(s.depth_fail));
			desc->setDepthStencilPassOperation(to_stencil_operation(s.pass));
			desc->setStencilCompareFunction(to_compare_function(s.comparison));
			desc->setReadMask(stencil_read);
			desc->setWriteMask(stencil_write);
			return desc;
		}

		MTL::Size to_size(cvec3<NS::UInteger> sz) {
			return MTL::Size(sz[0], sz[1], sz[2]);
		}

		MTL::PackedFloat4x3 to_packed_float4x3(mat34f32 m) {
			MTL::PackedFloat4x3 result;
			for (usize r = 0; r < 3; ++r) {
				for (usize c = 0; c < 4; ++c) {
					result[static_cast<int>(c)][static_cast<int>(r)] = m(r, c);
				}
			}
			return result;
		}

		MTL::Viewport to_viewport(viewport vp) {
			const cvec2f32 size = vp.xy.signed_size();
			return MTL::Viewport(vp.xy.min[0], vp.xy.min[1], size[0], size[1], vp.minimum_depth, vp.maximum_depth);
		}

		MTL::ScissorRect to_scissor_rect(aab2u32 rect) {
			const cvec2u32 size = rect.signed_size();
			return MTL::ScissorRect(rect.min[0], rect.min[1], size[0], size[1]);
		}


		std::u8string back_to_string(NS::String *str) {
			return std::u8string(
				reinterpret_cast<const char8_t*>(str->utf8String()),
				static_cast<usize>(str->lengthOfBytes(NS::UTF8StringEncoding))
			);
		}

		memory::size_alignment back_to_size_alignment(MTL::SizeAndAlign sa) {
			return memory::size_alignment(sa.size, sa.align);
		}

		acceleration_structure_build_sizes back_to_acceleration_structure_build_sizes(
			MTL::AccelerationStructureSizes sz
		) {
			acceleration_structure_build_sizes result = uninitialized;
			result.acceleration_structure_size = sz.accelerationStructureSize;
			result.build_scratch_size          = sz.buildScratchBufferSize;
			result.update_scratch_size         = sz.refitScratchBufferSize;
			return result;
		}

		IRDescriptorRange1 d3d12_shader_input_bind_desc_to_ir_descriptor_range(
			D3D12_SHADER_INPUT_BIND_DESC desc
		) {
			IRDescriptorRange1 result;
			result.RangeType                         = to_ir_descriptor_range_type(desc.Type);
			result.NumDescriptors                    =
				desc.BindCount == 0 ? std::numeric_limits<u32>::max() : desc.BindCount;
			result.BaseShaderRegister                = desc.BindPoint;
			result.RegisterSpace                     = desc.Space;
			result.Flags                             = IRDescriptorRangeFlagNone;
			result.OffsetInDescriptorsFromTableStart = desc.BindPoint;
			return result;
		}
	}

	NS::SharedPtr<MTL::TextureDescriptor> create_texture_descriptor(
		MTL::TextureType type,
		format fmt,
		cvec3u32 size,
		u32 mip_levels,
		MTL::ResourceOptions opts,
		image_usage_mask usages
	) {
		auto descriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
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


	bool does_pixel_format_have_depth(MTL::PixelFormat fmt) {
		return
			fmt == MTL::PixelFormatDepth16Unorm ||
			fmt == MTL::PixelFormatDepth32Float ||
			fmt == MTL::PixelFormatDepth24Unorm_Stencil8 ||
			fmt == MTL::PixelFormatDepth32Float_Stencil8;
	}

	bool does_pixel_format_have_stencil(MTL::PixelFormat fmt) {
		return
			fmt == MTL::PixelFormatDepth24Unorm_Stencil8 ||
			fmt == MTL::PixelFormatDepth32Float_Stencil8 ||
			fmt == MTL::PixelFormatStencil8 ||
			fmt == MTL::PixelFormatX24_Stencil8 ||
			fmt == MTL::PixelFormatX32_Stencil8;
	}


	NS::SharedPtr<MTL::Function> get_single_shader_function(MTL::Library *lib) {
		crash_if(lib->functionNames()->count() != 1);
		return NS::TransferPtr(lib->newFunction(lib->functionNames()->object<NS::String>(0)));
	}


	namespace shader {
		ir_unique_ptr<IRRootSignature> create_root_signature_for_bindings(std::span<IRRootParameter1> params) {
			IRVersionedRootSignatureDescriptor root_sig_desc = {};
			root_sig_desc.version                = IRRootSignatureVersion_1_1;
			root_sig_desc.desc_1_1.NumParameters = static_cast<u32>(params.size());
			root_sig_desc.desc_1_1.pParameters   = params.data();
			IRError *error = nullptr;
			auto root_signature = ir_make_unique(
				IRRootSignatureCreateFromDescriptor(&root_sig_desc, &error)
			);
			if (error) {
				log().error(
					"Failed to create IR root signature: {}", static_cast<const char*>(IRErrorGetPayload(error))
				);
				IRErrorDestroy(error);
				std::abort();
			}
			return root_signature;
		}

		ir_unique_ptr<IRRootSignature> create_root_signature_for_shader_reflection(ID3D12ShaderReflection *refl) {
			auto bookmark = get_scratch_bookmark();
			auto descriptor_spaces =
				bookmark.create_vector_array<memory::stack_allocator::vector_type<IRDescriptorRange1>>();

			D3D12_SHADER_DESC desc = {};
			common::_details::assert_dx(refl->GetDesc(&desc));
			for (UINT i = 0; i < desc.BoundResources; ++i) {
				D3D12_SHADER_INPUT_BIND_DESC binding = {};
				common::_details::assert_dx(refl->GetResourceBindingDesc(i, &binding));

				// record this descriptor range
				while (descriptor_spaces.size() <= binding.Space) {
					descriptor_spaces.emplace_back(bookmark.create_vector_array<IRDescriptorRange1>());
				}
				descriptor_spaces[binding.Space].emplace_back(
					conversions::d3d12_shader_input_bind_desc_to_ir_descriptor_range(binding)
				);
			}

			// create descriptor binding array
			auto descriptor_bindings = bookmark.create_vector_array<IRRootParameter1>(descriptor_spaces.size());
			for (usize i = 0; i < descriptor_spaces.size(); ++i) {
				descriptor_bindings[i].ParameterType                       = IRRootParameterTypeDescriptorTable;
				descriptor_bindings[i].DescriptorTable.NumDescriptorRanges =
					static_cast<u32>(descriptor_spaces[i].size());
				descriptor_bindings[i].DescriptorTable.pDescriptorRanges   = descriptor_spaces[i].data();
				descriptor_bindings[i].ShaderVisibility                    = IRShaderVisibilityAll;
			}

			return create_root_signature_for_bindings(descriptor_bindings);
		}

		ir_conversion_result convert_to_metal_ir(
			std::span<const std::byte> dxil_buf, IRRootSignature *root_sig
		) {
			ir_conversion_result result = nullptr;

			// convert
			auto dxil = ir_make_unique(IRObjectCreateFromDXIL(
				reinterpret_cast<const u8*>(dxil_buf.data()), dxil_buf.size(), IRBytecodeOwnershipNone
			));

			auto compiler = ir_make_unique(IRCompilerCreate());
			IRCompilerSetGlobalRootSignature(compiler.get(), root_sig);
			// TODO these are only available when building the pipeline
			IRCompilerSetRayTracingPipelineArguments(
				compiler.get(),
				128,
				IRRaytracingPipelineFlagNone,
				IRIntrinsicMaskClosestHitAll,
				IRIntrinsicMaskMissShaderAll,
				IRIntrinsicMaskAnyHitShaderAll,
				IRIntrinsicMaskCallableShaderAll,
				IRRayTracingUnlimitedRecursionDepth,
				IRRayGenerationCompilationVisibleFunction
			);
			IRError *error = nullptr;
			result.object = ir_make_unique(IRCompilerAllocCompileAndLink(
				compiler.get(), nullptr, dxil.get(), &error
			));
			if (error) {
				log().error("Failed to compile IR: {}", static_cast<const char*>(IRErrorGetPayload(error)));
				IRErrorDestroy(error);
				std::abort();
			}

			// retrieve Metal lib and bytes
			const IRShaderStage stage = IRObjectGetMetalIRShaderStage(result.object.get());
			auto lib = ir_make_unique(IRMetalLibBinaryCreate());
			IRObjectGetMetalLibBinary(result.object.get(), stage, lib.get());
			result.data = IRMetalLibGetBytecodeData(lib.get()); // "Do not release this object"

			return result;
		}
	}
}
