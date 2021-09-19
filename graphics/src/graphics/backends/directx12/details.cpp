#include "lotus/graphics/backends/directx12/details.h"

/// \file
/// Implementation of miscellaneous DirectX 12 related functions.

#include <comdef.h>

#include "lotus/system/platforms/windows/details.h"
#include "lotus/graphics/backends/directx12/device.h"

namespace lotus::graphics::backends::directx12::_details {
	void assert_dx(HRESULT hr) {
		if (FAILED(hr)) {
			_com_error err(hr);
			auto msg = system::platforms::windows::_details::tstring_to_u8string(
				err.ErrorMessage(), std::allocator<char8_t>()
			);
			std::cerr <<
				"DirectX error " <<
				std::hex << hr << ": " <<
				std::string_view(reinterpret_cast<const char*>(msg.c_str()), msg.size()) <<
				std::endl;
			std::abort();
		}
	}


	namespace conversions {
		DXGI_FORMAT for_format(format fmt) {
			constexpr static enum_mapping<format, DXGI_FORMAT> table{
				std::pair(format::none,                 DXGI_FORMAT_UNKNOWN              ),
				std::pair(format::d32_float_s8,         DXGI_FORMAT_D32_FLOAT_S8X24_UINT ),
				std::pair(format::d32_float,            DXGI_FORMAT_D32_FLOAT            ),
				std::pair(format::d24_unorm_s8,         DXGI_FORMAT_D24_UNORM_S8_UINT    ),
				std::pair(format::d16_unorm,            DXGI_FORMAT_D16_UNORM            ),
				std::pair(format::r8_unorm,             DXGI_FORMAT_R8_UNORM             ),
				std::pair(format::r8_snorm,             DXGI_FORMAT_R8_SNORM             ),
				std::pair(format::r8_uint,              DXGI_FORMAT_R8_UINT              ),
				std::pair(format::r8_sint,              DXGI_FORMAT_R8_SINT              ),
				std::pair(format::r8_unknown,           DXGI_FORMAT_R8_TYPELESS          ),
				std::pair(format::r8g8_unorm,           DXGI_FORMAT_R8G8_UNORM           ),
				std::pair(format::r8g8_snorm,           DXGI_FORMAT_R8G8_SNORM           ),
				std::pair(format::r8g8_uint,            DXGI_FORMAT_R8G8_UINT            ),
				std::pair(format::r8g8_sint,            DXGI_FORMAT_R8G8_SINT            ),
				std::pair(format::r8g8_unknown,         DXGI_FORMAT_R8G8_TYPELESS        ),
				std::pair(format::r8g8b8a8_unorm,       DXGI_FORMAT_R8G8B8A8_UNORM       ),
				std::pair(format::r8g8b8a8_snorm,       DXGI_FORMAT_R8G8B8A8_SNORM       ),
				std::pair(format::r8g8b8a8_srgb,        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB  ),
				std::pair(format::r8g8b8a8_uint,        DXGI_FORMAT_R8G8B8A8_UINT        ),
				std::pair(format::r8g8b8a8_sint,        DXGI_FORMAT_R8G8B8A8_SINT        ),
				std::pair(format::r8g8b8a8_unknown,     DXGI_FORMAT_R8G8B8A8_TYPELESS    ),
				std::pair(format::r16_unorm,            DXGI_FORMAT_R16_UNORM            ),
				std::pair(format::r16_snorm,            DXGI_FORMAT_R16_SNORM            ),
				std::pair(format::r16_uint,             DXGI_FORMAT_R16_UINT             ),
				std::pair(format::r16_sint,             DXGI_FORMAT_R16_SINT             ),
				std::pair(format::r16_float,            DXGI_FORMAT_R16_FLOAT            ),
				std::pair(format::r16_unknown,          DXGI_FORMAT_R16_TYPELESS         ),
				std::pair(format::r16g16_unorm,         DXGI_FORMAT_R16G16_UNORM         ),
				std::pair(format::r16g16_snorm,         DXGI_FORMAT_R16G16_SNORM         ),
				std::pair(format::r16g16_uint,          DXGI_FORMAT_R16G16_UINT          ),
				std::pair(format::r16g16_sint,          DXGI_FORMAT_R16G16_SINT          ),
				std::pair(format::r16g16_float,         DXGI_FORMAT_R16G16_FLOAT         ),
				std::pair(format::r16g16_unknown,       DXGI_FORMAT_R16G16_TYPELESS      ),
				std::pair(format::r16g16b16a16_unorm,   DXGI_FORMAT_R16G16B16A16_UNORM   ),
				std::pair(format::r16g16b16a16_snorm,   DXGI_FORMAT_R16G16B16A16_SNORM   ),
				std::pair(format::r16g16b16a16_uint,    DXGI_FORMAT_R16G16B16A16_UINT    ),
				std::pair(format::r16g16b16a16_sint,    DXGI_FORMAT_R16G16B16A16_SINT    ),
				std::pair(format::r16g16b16a16_float,   DXGI_FORMAT_R16G16B16A16_FLOAT   ),
				std::pair(format::r16g16b16a16_unknown, DXGI_FORMAT_R16G16B16A16_TYPELESS),
				std::pair(format::r32_uint,             DXGI_FORMAT_R32_UINT             ),
				std::pair(format::r32_sint,             DXGI_FORMAT_R32_SINT             ),
				std::pair(format::r32_float,            DXGI_FORMAT_R32_FLOAT            ),
				std::pair(format::r32_unknown,          DXGI_FORMAT_R32_TYPELESS         ),
				std::pair(format::r32g32_uint,          DXGI_FORMAT_R32G32_UINT          ),
				std::pair(format::r32g32_sint,          DXGI_FORMAT_R32G32_SINT          ),
				std::pair(format::r32g32_float,         DXGI_FORMAT_R32G32_FLOAT         ),
				std::pair(format::r32g32_unknown,       DXGI_FORMAT_R32G32_TYPELESS      ),
				std::pair(format::r32g32b32_uint,       DXGI_FORMAT_R32G32B32_UINT       ),
				std::pair(format::r32g32b32_sint,       DXGI_FORMAT_R32G32B32_SINT       ),
				std::pair(format::r32g32b32_float,      DXGI_FORMAT_R32G32B32_FLOAT      ),
				std::pair(format::r32g32b32_unknown,    DXGI_FORMAT_R32G32B32_TYPELESS   ),
				std::pair(format::r32g32b32a32_uint,    DXGI_FORMAT_R32G32B32A32_UINT    ),
				std::pair(format::r32g32b32a32_sint,    DXGI_FORMAT_R32G32B32A32_SINT    ),
				std::pair(format::r32g32b32a32_float,   DXGI_FORMAT_R32G32B32A32_FLOAT   ),
				std::pair(format::r32g32b32a32_unknown, DXGI_FORMAT_R32G32B32A32_TYPELESS),
			};
			return table[fmt];
		}

		D3D12_TEXTURE_LAYOUT for_image_tiling(image_tiling tiling) {
			constexpr static enum_mapping<image_tiling, D3D12_TEXTURE_LAYOUT> table{
				std::pair(image_tiling::row_major, D3D12_TEXTURE_LAYOUT_ROW_MAJOR),
				std::pair(image_tiling::optimal,   D3D12_TEXTURE_LAYOUT_UNKNOWN  ),
			};
			return table[tiling];
		}

		D3D12_BLEND for_blend_factor(blend_factor factor) {
			constexpr static enum_mapping<blend_factor, D3D12_BLEND> table{
				std::pair(blend_factor::zero,                        D3D12_BLEND_ZERO          ),
				std::pair(blend_factor::one,                         D3D12_BLEND_ONE           ),
				std::pair(blend_factor::source_color,                D3D12_BLEND_SRC_COLOR     ),
				std::pair(blend_factor::one_minus_source_color,      D3D12_BLEND_INV_SRC_COLOR ),
				std::pair(blend_factor::destination_color,           D3D12_BLEND_DEST_COLOR    ),
				std::pair(blend_factor::one_minus_destination_color, D3D12_BLEND_INV_DEST_COLOR),
				std::pair(blend_factor::source_alpha,                D3D12_BLEND_SRC_ALPHA     ),
				std::pair(blend_factor::one_minus_source_alpha,      D3D12_BLEND_INV_SRC_ALPHA ),
				std::pair(blend_factor::destination_alpha,           D3D12_BLEND_DEST_ALPHA    ),
				std::pair(blend_factor::one_minus_destination_alpha, D3D12_BLEND_INV_DEST_ALPHA),
			};
			return table[factor];
		}

		D3D12_BLEND_OP for_blend_operation(blend_operation op) {
			constexpr static enum_mapping<blend_operation, D3D12_BLEND_OP> table{
				std::pair(blend_operation::add,              D3D12_BLEND_OP_ADD         ),
				std::pair(blend_operation::subtract,         D3D12_BLEND_OP_SUBTRACT    ),
				std::pair(blend_operation::reverse_subtract, D3D12_BLEND_OP_REV_SUBTRACT),
				std::pair(blend_operation::min,              D3D12_BLEND_OP_MIN         ),
				std::pair(blend_operation::max,              D3D12_BLEND_OP_MAX         ),
			};
			return table[op];
		}

		D3D12_CULL_MODE for_cull_mode(cull_mode mode) {
			constexpr static enum_mapping<cull_mode, D3D12_CULL_MODE> table{
				std::pair(cull_mode::none,       D3D12_CULL_MODE_NONE ),
				std::pair(cull_mode::cull_front, D3D12_CULL_MODE_FRONT),
				std::pair(cull_mode::cull_back,  D3D12_CULL_MODE_BACK ),
			};
			return table[mode];
		}

		D3D12_STENCIL_OP for_stencil_operation(stencil_operation op) {
			constexpr static enum_mapping<stencil_operation, D3D12_STENCIL_OP> table{
				std::pair(stencil_operation::keep,                D3D12_STENCIL_OP_KEEP    ),
				std::pair(stencil_operation::zero,                D3D12_STENCIL_OP_ZERO    ),
				std::pair(stencil_operation::replace,             D3D12_STENCIL_OP_REPLACE ),
				std::pair(stencil_operation::increment_and_clamp, D3D12_STENCIL_OP_INCR_SAT),
				std::pair(stencil_operation::decrement_and_clamp, D3D12_STENCIL_OP_DECR_SAT),
				std::pair(stencil_operation::bitwise_invert,      D3D12_STENCIL_OP_INVERT  ),
				std::pair(stencil_operation::increment_and_wrap,  D3D12_STENCIL_OP_INCR    ),
				std::pair(stencil_operation::decrement_and_wrap,  D3D12_STENCIL_OP_DECR    ),
			};
			return table[op];
		}

		D3D12_INPUT_CLASSIFICATION for_input_buffer_rate(input_buffer_rate rate) {
			constexpr static enum_mapping<input_buffer_rate, D3D12_INPUT_CLASSIFICATION> table{
				std::pair(input_buffer_rate::per_vertex,   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA  ),
				std::pair(input_buffer_rate::per_instance, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA),
			};
			return table[rate];
		}

		D3D12_PRIMITIVE_TOPOLOGY_TYPE for_primitive_topology_type(primitive_topology topology) {
			constexpr static enum_mapping<primitive_topology, D3D12_PRIMITIVE_TOPOLOGY_TYPE> table{
				std::pair(primitive_topology::point_list,                    D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT   ),
				std::pair(primitive_topology::line_list,                     D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE    ),
				std::pair(primitive_topology::line_strip,                    D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE    ),
				std::pair(primitive_topology::triangle_list,                 D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE),
				std::pair(primitive_topology::triangle_strip,                D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE),
				std::pair(primitive_topology::line_list_with_adjacency,      D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE    ),
				std::pair(primitive_topology::line_strip_with_adjacency,     D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE    ),
				std::pair(primitive_topology::triangle_list_with_adjacency,  D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE),
				std::pair(primitive_topology::triangle_strip_with_adjacency, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE),
			};
			return table[topology];
		}

		D3D_PRIMITIVE_TOPOLOGY for_primitive_topology(primitive_topology topology) {
			constexpr static enum_mapping<primitive_topology, D3D_PRIMITIVE_TOPOLOGY> table{
				std::pair(primitive_topology::point_list,                    D3D_PRIMITIVE_TOPOLOGY_POINTLIST        ),
				std::pair(primitive_topology::line_list,                     D3D_PRIMITIVE_TOPOLOGY_LINELIST         ),
				std::pair(primitive_topology::line_strip,                    D3D_PRIMITIVE_TOPOLOGY_LINESTRIP        ),
				std::pair(primitive_topology::triangle_list,                 D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST     ),
				std::pair(primitive_topology::triangle_strip,                D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP    ),
				std::pair(primitive_topology::line_list_with_adjacency,      D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ     ),
				std::pair(primitive_topology::line_strip_with_adjacency,     D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ    ),
				std::pair(primitive_topology::triangle_list_with_adjacency,  D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ ),
				std::pair(primitive_topology::triangle_strip_with_adjacency, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ),
			};
			return table[topology];
		}

		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE for_pass_load_operation(pass_load_operation op) {
			constexpr static enum_mapping<pass_load_operation, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE> table{
				std::pair(pass_load_operation::discard,  D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD ),
				std::pair(pass_load_operation::preserve, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE),
				std::pair(pass_load_operation::clear,    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR   ),
			};
			return table[op];
		}

		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE for_pass_store_operation(pass_store_operation op) {
			constexpr static enum_mapping<pass_store_operation, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE> table{
				std::pair(pass_store_operation::discard,  D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD ),
				std::pair(pass_store_operation::preserve, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE),
			};
			return table[op];
		}

		D3D12_DESCRIPTOR_RANGE_TYPE for_descriptor_type(descriptor_type ty) {
			constexpr static enum_mapping<descriptor_type, D3D12_DESCRIPTOR_RANGE_TYPE> table{
				std::pair(descriptor_type::sampler,           D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER),
				std::pair(descriptor_type::read_only_image,   D3D12_DESCRIPTOR_RANGE_TYPE_SRV    ),
				std::pair(descriptor_type::read_write_image,  D3D12_DESCRIPTOR_RANGE_TYPE_UAV    ),
				std::pair(descriptor_type::read_only_buffer,  D3D12_DESCRIPTOR_RANGE_TYPE_SRV    ),
				std::pair(descriptor_type::read_write_buffer, D3D12_DESCRIPTOR_RANGE_TYPE_UAV    ),
			};
			return table[ty];
		}

		D3D12_RESOURCE_STATES for_image_usage(image_usage st) {
			constexpr static enum_mapping<image_usage, D3D12_RESOURCE_STATES> table{
				std::pair(image_usage::color_render_target,         D3D12_RESOURCE_STATE_RENDER_TARGET),
				std::pair(image_usage::depth_stencil_render_target, D3D12_RESOURCE_STATE_DEPTH_WRITE  ),
				std::pair(image_usage::read_only_texture,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE                                        |
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE                                    ),
				std::pair(image_usage::read_write_color_texture,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS                                             |
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE                                        |
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE                                    ),
				std::pair(image_usage::present,                     D3D12_RESOURCE_STATE_PRESENT      ),
				std::pair(image_usage::copy_source,                 D3D12_RESOURCE_STATE_COPY_SOURCE  ),
				std::pair(image_usage::copy_destination,            D3D12_RESOURCE_STATE_COPY_DEST    ),
			};
			return table[st];
		}

		D3D12_RESOURCE_STATES for_buffer_usage(buffer_usage st) {
			constexpr static enum_mapping<buffer_usage, D3D12_RESOURCE_STATES> table{
				std::pair(buffer_usage::index_buffer,     D3D12_RESOURCE_STATE_INDEX_BUFFER              ),
				std::pair(buffer_usage::vertex_buffer,    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
				std::pair(buffer_usage::read_only_buffer,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER                                      |
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE                                           |
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE                                       ),
				std::pair(buffer_usage::read_write_buffer,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS                                                |
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE                                           |
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE                                       ),
				std::pair(buffer_usage::copy_source,      D3D12_RESOURCE_STATE_COPY_SOURCE               ),
				std::pair(buffer_usage::copy_destination, D3D12_RESOURCE_STATE_COPY_DEST                 ),
			};
			return table[st];
		}

		D3D12_HEAP_TYPE for_heap_type(heap_type ty) {
			constexpr static enum_mapping<heap_type, D3D12_HEAP_TYPE> table{
				std::pair(heap_type::device_only, D3D12_HEAP_TYPE_DEFAULT ),
				std::pair(heap_type::upload,      D3D12_HEAP_TYPE_UPLOAD  ),
				std::pair(heap_type::readback,    D3D12_HEAP_TYPE_READBACK),
			};
			return table[ty];
		}

		D3D12_TEXTURE_ADDRESS_MODE for_sampler_address_mode(sampler_address_mode mode) {
			constexpr static enum_mapping<sampler_address_mode, D3D12_TEXTURE_ADDRESS_MODE> table{
				std::pair(sampler_address_mode::repeat, D3D12_TEXTURE_ADDRESS_MODE_WRAP  ),
				std::pair(sampler_address_mode::mirror, D3D12_TEXTURE_ADDRESS_MODE_MIRROR),
				std::pair(sampler_address_mode::clamp,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP ),
				std::pair(sampler_address_mode::border, D3D12_TEXTURE_ADDRESS_MODE_BORDER),
			};
			return table[mode];
		}

		D3D12_COMPARISON_FUNC for_comparison_function(comparison_function mode) {
			constexpr static enum_mapping<comparison_function, D3D12_COMPARISON_FUNC> table{
				std::pair(comparison_function::never,            D3D12_COMPARISON_FUNC_NEVER        ),
				std::pair(comparison_function::less,             D3D12_COMPARISON_FUNC_LESS         ),
				std::pair(comparison_function::equal,            D3D12_COMPARISON_FUNC_EQUAL        ),
				std::pair(comparison_function::less_or_equal,    D3D12_COMPARISON_FUNC_LESS_EQUAL   ),
				std::pair(comparison_function::greater,          D3D12_COMPARISON_FUNC_GREATER      ),
				std::pair(comparison_function::not_equal,        D3D12_COMPARISON_FUNC_NOT_EQUAL    ),
				std::pair(comparison_function::greater_or_equal, D3D12_COMPARISON_FUNC_GREATER_EQUAL),
				std::pair(comparison_function::always,           D3D12_COMPARISON_FUNC_ALWAYS       ),
			};
			return table[mode];
		}


		D3D12_COLOR_WRITE_ENABLE for_channel_mask(channel_mask mask) {
			static const std::pair<channel_mask, D3D12_COLOR_WRITE_ENABLE> table[]{
				{ channel_mask::red,   D3D12_COLOR_WRITE_ENABLE_RED   },
				{ channel_mask::green, D3D12_COLOR_WRITE_ENABLE_GREEN },
				{ channel_mask::blue,  D3D12_COLOR_WRITE_ENABLE_BLUE  },
				{ channel_mask::alpha, D3D12_COLOR_WRITE_ENABLE_ALPHA },
			};
			std::uint8_t result = 0;
			for (auto [myval, dxval] : table) {
				if ((mask & myval) == myval) {
					result |= dxval;
				}
			}
			return static_cast<D3D12_COLOR_WRITE_ENABLE>(result);
		}

		D3D12_SHADER_VISIBILITY for_shader_stage_mask(shader_stage_mask mask) {
			static const std::pair<shader_stage_mask, D3D12_SHADER_VISIBILITY> table[]{
				{ shader_stage_mask::vertex_shader,   D3D12_SHADER_VISIBILITY_VERTEX   },
				{ shader_stage_mask::geometry_shader, D3D12_SHADER_VISIBILITY_GEOMETRY },
				{ shader_stage_mask::pixel_shader,    D3D12_SHADER_VISIBILITY_PIXEL    },
				{ shader_stage_mask::compute_shader,  D3D12_SHADER_VISIBILITY_ALL      },
			};
			std::uint8_t result = 0;
			for (auto [myval, dxval] : table) {
				if ((mask & myval) == myval) {
					result |= dxval;
				}
			}
			return static_cast<D3D12_SHADER_VISIBILITY>(result);
		}


		D3D12_FILTER for_filtering(
			filtering minification, filtering magnification, filtering mipmapping, bool anisotropic, bool comparison
		) {
			constexpr auto _num_filtering_types = static_cast<std::size_t>(filtering::num_enumerators);
			//                                     minification          magnification         mipmapping
			using _table_type = const D3D12_FILTER[_num_filtering_types][_num_filtering_types][_num_filtering_types];
			static _table_type non_comparison_table{
				{
					{ D3D12_FILTER_MIN_MAG_MIP_POINT,              D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR        },
					{ D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR        },
				}, {
					{ D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT,       D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR },
					{ D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,       D3D12_FILTER_MIN_MAG_MIP_LINEAR              },
				}
			};
			static _table_type comparison_table{
				{
					{ D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,              D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR        },
					{ D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR        },
				}, {
					{ D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT,       D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR },
					{ D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,       D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR              },
				}
			};
			static_assert(
				static_cast<std::size_t>(filtering::num_enumerators) == 2,
				"Table for filtering type conversion needs updating"
			);
			if (anisotropic) {
				return comparison ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC;
			}
			_table_type &table = comparison ? comparison_table : non_comparison_table;
			return table
				[static_cast<std::size_t>(minification)]
				[static_cast<std::size_t>(magnification)]
				[static_cast<std::size_t>(mipmapping)];
		}


		D3D12_VIEWPORT for_viewport(const viewport &vp) {
			cvec2f size = vp.xy.signed_size();
			D3D12_VIEWPORT result = {};
			result.TopLeftX = vp.xy.min[0];
			result.TopLeftY = vp.xy.min[1];
			result.Width    = size[0];
			result.Height   = size[1];
			result.MinDepth = vp.minimum_depth;
			result.MaxDepth = vp.maximum_depth;
			return result;
		}

		D3D12_RECT for_rect(const aab2i &rect) {
			D3D12_RECT result = {};
			result.left   = static_cast<LONG>(rect.min[0]);
			result.top    = static_cast<LONG>(rect.min[1]);
			result.right  = static_cast<LONG>(rect.max[0]);
			result.bottom = static_cast<LONG>(rect.max[1]);
			return result;
		}


		D3D12_RENDER_TARGET_BLEND_DESC for_render_target_blend_options(
			const render_target_blend_options &opt
		) {
			D3D12_RENDER_TARGET_BLEND_DESC result = {};
			result.BlendEnable           = opt.enabled;
			result.SrcBlend              = for_blend_factor(opt.source_color);
			result.DestBlend             = for_blend_factor(opt.destination_color);
			result.BlendOp               = for_blend_operation(opt.color_operation);
			result.SrcBlendAlpha         = for_blend_factor(opt.source_alpha);
			result.DestBlendAlpha        = for_blend_factor(opt.destination_alpha);
			result.BlendOp               = for_blend_operation(opt.color_operation);
			result.RenderTargetWriteMask = static_cast<UINT8>(for_channel_mask(opt.write_mask));
			return result;
		}

		D3D12_BLEND_DESC for_blend_options(const blend_options &opt) {
			D3D12_BLEND_DESC result = {};
			// TODO handle logic operations
			constexpr std::size_t _num_render_targets =
				std::min(std::size(result.RenderTarget), num_color_render_targets);
			result.IndependentBlendEnable = true;
			for (std::size_t i = 0; i < _num_render_targets; ++i) {
				result.RenderTarget[i] = for_render_target_blend_options(opt.render_target_options[i]);
			}
			return result;
		}

		D3D12_RASTERIZER_DESC for_rasterizer_options(const rasterizer_options &opt) {
			D3D12_RASTERIZER_DESC result = {};
			result.FillMode              = opt.is_wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
			result.CullMode              = for_cull_mode(opt.culling);
			result.FrontCounterClockwise = opt.front_facing == front_facing_mode::counter_clockwise;
			result.DepthBias             = static_cast<INT>(std::round(opt.depth_bias.bias));
			result.DepthBiasClamp        = opt.depth_bias.clamp;
			result.SlopeScaledDepthBias  = opt.depth_bias.slope_scaled_bias;
			result.DepthClipEnable       = false; // TODO
			result.MultisampleEnable     = false; // TODO
			result.AntialiasedLineEnable = false;
			result.ForcedSampleCount     = 0;
			result.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
			return result;
		}

		D3D12_DEPTH_STENCILOP_DESC for_stencil_options(const stencil_options &op) {
			D3D12_DEPTH_STENCILOP_DESC result = {};
			result.StencilFailOp      = for_stencil_operation(op.fail);
			result.StencilDepthFailOp = for_stencil_operation(op.depth_fail);
			result.StencilPassOp      = for_stencil_operation(op.pass);
			result.StencilFunc        = for_comparison_function(op.comparison);
			return result;
		}

		D3D12_DEPTH_STENCIL_DESC for_depth_stencil_options(const depth_stencil_options &opt) {
			D3D12_DEPTH_STENCIL_DESC result = {};
			result.DepthEnable      = opt.enable_depth_testing;
			result.DepthWriteMask   = opt.write_depth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
			result.DepthFunc        = for_comparison_function(opt.depth_comparison);
			result.StencilEnable    = opt.enable_stencil_testing;
			result.StencilReadMask  = opt.stencil_read_mask;
			result.StencilWriteMask = opt.stencil_write_mask;
			result.FrontFace        = for_stencil_options(opt.stencil_front_face);
			result.BackFace         = for_stencil_options(opt.stencil_back_face);
			return result;
		}

		D3D12_RENDER_PASS_RENDER_TARGET_DESC for_render_target_pass_options(
			const render_target_pass_options &opt
		) {
			D3D12_RENDER_PASS_RENDER_TARGET_DESC result = {};
			result.BeginningAccess.Clear.ClearValue.Format = for_format(opt.pixel_format);
			result.BeginningAccess.Type                    = for_pass_load_operation(opt.load_operation);
			result.EndingAccess.Type                       = for_pass_store_operation(opt.store_operation);
			return result;
		}

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC for_depth_stencil_pass_options(
			const depth_stencil_pass_options &opt
		) {
			D3D12_RENDER_PASS_DEPTH_STENCIL_DESC result = {};
			DXGI_FORMAT format = for_format(opt.pixel_format);
			if (format_properties::get(opt.pixel_format).depth_bits > 0) {
				result.DepthBeginningAccess.Clear.ClearValue.Format = format;
				result.DepthBeginningAccess.Type = for_pass_load_operation(opt.depth_load_operation);
				result.DepthEndingAccess.Type    = for_pass_store_operation(opt.depth_store_operation);
			} else {
				result.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
				result.DepthEndingAccess.Type    = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			}
			if (format_properties::get(opt.pixel_format).stencil_bits > 0) {
				result.StencilBeginningAccess.Clear.ClearValue.Format = format;
				result.StencilBeginningAccess.Type = for_pass_load_operation(opt.stencil_load_operation);
				result.StencilEndingAccess.Type    = for_pass_store_operation(opt.stencil_store_operation);
			} else {
				result.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
				result.StencilEndingAccess.Type    = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			}
			return result;
		}
	}


	D3D12_HEAP_PROPERTIES heap_type_to_properties(heap_type type) {
		D3D12_HEAP_PROPERTIES result = {};
		result.Type                 = conversions::for_heap_type(type);
		result.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		result.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		result.CreationNodeMask     = 0;
		result.VisibleNodeMask      = 0;
		return result;
	}


	namespace resource_desc {
		D3D12_RESOURCE_DESC for_buffer(std::size_t size) {
			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			desc.Width              = size;
			desc.Height             = 1;
			desc.DepthOrArraySize   = 1;
			desc.MipLevels          = 1;
			desc.Format             = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags              = D3D12_RESOURCE_FLAG_NONE;
			return desc;
		}

		void adjust_resource_flags_for_buffer(
			heap_type type, buffer_usage::mask all_usages, buffer_usage initial_usage,
			D3D12_RESOURCE_DESC &desc, D3D12_RESOURCE_STATES &states, D3D12_HEAP_FLAGS *heap_flags
		) {
			if (type == heap_type::device_only) {
				if (!is_empty(all_usages & buffer_usage::mask::read_write_buffer)) {
					desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
					if (heap_flags) {
						*heap_flags |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;	
					}
				}
			} else if (type == heap_type::upload) {
				assert(initial_usage == buffer_usage::copy_source);
				assert(all_usages == buffer_usage::mask::copy_source);
				states = D3D12_RESOURCE_STATE_GENERIC_READ;
			}
		}

		D3D12_RESOURCE_DESC for_image2d(
			std::size_t width, std::size_t height, std::size_t array_slices,
			std::size_t mip_levels, format fmt, image_tiling tiling
		) {
			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			desc.Width              = static_cast<UINT64>(width);
			desc.Height             = static_cast<UINT>(height);
			desc.DepthOrArraySize   = static_cast<UINT16>(array_slices);
			desc.MipLevels          = static_cast<UINT16>(mip_levels);
			desc.Format             = conversions::for_format(fmt);
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout             = conversions::for_image_tiling(tiling);
			desc.Flags              = D3D12_RESOURCE_FLAG_NONE;
			return desc;
		}

		void adjust_resource_flags_for_image2d(
			format, image_usage::mask all_usages, D3D12_RESOURCE_DESC &desc, D3D12_HEAP_FLAGS *heap_flags
		) {
			if (!is_empty(all_usages & image_usage::mask::read_write_color_texture)) {
				desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				if (heap_flags) {
					*heap_flags |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
				}
			}
			if (!is_empty(all_usages & image_usage::mask::color_render_target)) {
				desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			}
			if (!is_empty(all_usages & image_usage::mask::depth_stencil_render_target)) {
				desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			}
		}
	}
}
