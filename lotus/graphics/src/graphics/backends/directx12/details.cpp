#include "lotus/graphics/backends/directx12/details.h"

/// \file
/// Implementation of miscellaneous DirectX 12 related functions.

#include <mutex>

#include <comdef.h>
#include <directx/d3dx12.h>

#include "lotus/memory.h"
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
		DXGI_FORMAT to_format(format fmt) {
			constexpr static enum_mapping<format, DXGI_FORMAT> table{
				std::pair(format::none,               DXGI_FORMAT_UNKNOWN             ),
				std::pair(format::d32_float_s8,       DXGI_FORMAT_D32_FLOAT_S8X24_UINT),
				std::pair(format::d32_float,          DXGI_FORMAT_D32_FLOAT           ),
				std::pair(format::d24_unorm_s8,       DXGI_FORMAT_D24_UNORM_S8_UINT   ),
				std::pair(format::d16_unorm,          DXGI_FORMAT_D16_UNORM           ),
				std::pair(format::r8_unorm,           DXGI_FORMAT_R8_UNORM            ),
				std::pair(format::r8_snorm,           DXGI_FORMAT_R8_SNORM            ),
				std::pair(format::r8_uint,            DXGI_FORMAT_R8_UINT             ),
				std::pair(format::r8_sint,            DXGI_FORMAT_R8_SINT             ),
				std::pair(format::r8g8_unorm,         DXGI_FORMAT_R8G8_UNORM          ),
				std::pair(format::r8g8_snorm,         DXGI_FORMAT_R8G8_SNORM          ),
				std::pair(format::r8g8_uint,          DXGI_FORMAT_R8G8_UINT           ),
				std::pair(format::r8g8_sint,          DXGI_FORMAT_R8G8_SINT           ),
				std::pair(format::r8g8b8a8_unorm,     DXGI_FORMAT_R8G8B8A8_UNORM      ),
				std::pair(format::r8g8b8a8_snorm,     DXGI_FORMAT_R8G8B8A8_SNORM      ),
				std::pair(format::r8g8b8a8_srgb,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ),
				std::pair(format::r8g8b8a8_uint,      DXGI_FORMAT_R8G8B8A8_UINT       ),
				std::pair(format::r8g8b8a8_sint,      DXGI_FORMAT_R8G8B8A8_SINT       ),
				std::pair(format::b8g8r8a8_unorm,     DXGI_FORMAT_B8G8R8A8_UNORM      ),
				std::pair(format::b8g8r8a8_srgb,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ),
				std::pair(format::r16_unorm,          DXGI_FORMAT_R16_UNORM           ),
				std::pair(format::r16_snorm,          DXGI_FORMAT_R16_SNORM           ),
				std::pair(format::r16_uint,           DXGI_FORMAT_R16_UINT            ),
				std::pair(format::r16_sint,           DXGI_FORMAT_R16_SINT            ),
				std::pair(format::r16_float,          DXGI_FORMAT_R16_FLOAT           ),
				std::pair(format::r16g16_unorm,       DXGI_FORMAT_R16G16_UNORM        ),
				std::pair(format::r16g16_snorm,       DXGI_FORMAT_R16G16_SNORM        ),
				std::pair(format::r16g16_uint,        DXGI_FORMAT_R16G16_UINT         ),
				std::pair(format::r16g16_sint,        DXGI_FORMAT_R16G16_SINT         ),
				std::pair(format::r16g16_float,       DXGI_FORMAT_R16G16_FLOAT        ),
				std::pair(format::r16g16b16a16_unorm, DXGI_FORMAT_R16G16B16A16_UNORM  ),
				std::pair(format::r16g16b16a16_snorm, DXGI_FORMAT_R16G16B16A16_SNORM  ),
				std::pair(format::r16g16b16a16_uint,  DXGI_FORMAT_R16G16B16A16_UINT   ),
				std::pair(format::r16g16b16a16_sint,  DXGI_FORMAT_R16G16B16A16_SINT   ),
				std::pair(format::r16g16b16a16_float, DXGI_FORMAT_R16G16B16A16_FLOAT  ),
				std::pair(format::r32_uint,           DXGI_FORMAT_R32_UINT            ),
				std::pair(format::r32_sint,           DXGI_FORMAT_R32_SINT            ),
				std::pair(format::r32_float,          DXGI_FORMAT_R32_FLOAT           ),
				std::pair(format::r32g32_uint,        DXGI_FORMAT_R32G32_UINT         ),
				std::pair(format::r32g32_sint,        DXGI_FORMAT_R32G32_SINT         ),
				std::pair(format::r32g32_float,       DXGI_FORMAT_R32G32_FLOAT        ),
				std::pair(format::r32g32b32_uint,     DXGI_FORMAT_R32G32B32_UINT      ),
				std::pair(format::r32g32b32_sint,     DXGI_FORMAT_R32G32B32_SINT      ),
				std::pair(format::r32g32b32_float,    DXGI_FORMAT_R32G32B32_FLOAT     ),
				std::pair(format::r32g32b32a32_uint,  DXGI_FORMAT_R32G32B32A32_UINT   ),
				std::pair(format::r32g32b32a32_sint,  DXGI_FORMAT_R32G32B32A32_SINT   ),
				std::pair(format::r32g32b32a32_float, DXGI_FORMAT_R32G32B32A32_FLOAT  ),
			};
			return table[fmt];
		}

		DXGI_FORMAT to_format(index_format fmt) {
			constexpr static enum_mapping<index_format, DXGI_FORMAT> table{
				std::pair(index_format::uint16, DXGI_FORMAT_R16_UINT),
				std::pair(index_format::uint32, DXGI_FORMAT_R32_UINT),
			};
			return table[fmt];
		}

		D3D12_TEXTURE_LAYOUT to_texture_layout(image_tiling tiling) {
			constexpr static enum_mapping<image_tiling, D3D12_TEXTURE_LAYOUT> table{
				std::pair(image_tiling::row_major, D3D12_TEXTURE_LAYOUT_ROW_MAJOR),
				std::pair(image_tiling::optimal,   D3D12_TEXTURE_LAYOUT_UNKNOWN  ),
			};
			return table[tiling];
		}

		D3D12_BLEND to_blend_factor(blend_factor factor) {
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

		D3D12_BLEND_OP to_blend_operation(blend_operation op) {
			constexpr static enum_mapping<blend_operation, D3D12_BLEND_OP> table{
				std::pair(blend_operation::add,              D3D12_BLEND_OP_ADD         ),
				std::pair(blend_operation::subtract,         D3D12_BLEND_OP_SUBTRACT    ),
				std::pair(blend_operation::reverse_subtract, D3D12_BLEND_OP_REV_SUBTRACT),
				std::pair(blend_operation::min,              D3D12_BLEND_OP_MIN         ),
				std::pair(blend_operation::max,              D3D12_BLEND_OP_MAX         ),
			};
			return table[op];
		}

		D3D12_CULL_MODE to_cull_mode(cull_mode mode) {
			constexpr static enum_mapping<cull_mode, D3D12_CULL_MODE> table{
				std::pair(cull_mode::none,       D3D12_CULL_MODE_NONE ),
				std::pair(cull_mode::cull_front, D3D12_CULL_MODE_FRONT),
				std::pair(cull_mode::cull_back,  D3D12_CULL_MODE_BACK ),
			};
			return table[mode];
		}

		D3D12_STENCIL_OP to_stencil_operation(stencil_operation op) {
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

		D3D12_INPUT_CLASSIFICATION to_input_classification(input_buffer_rate rate) {
			constexpr static enum_mapping<input_buffer_rate, D3D12_INPUT_CLASSIFICATION> table{
				std::pair(input_buffer_rate::per_vertex,   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA  ),
				std::pair(input_buffer_rate::per_instance, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA),
			};
			return table[rate];
		}

		D3D12_PRIMITIVE_TOPOLOGY_TYPE to_primitive_topology_type(primitive_topology topology) {
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

		D3D_PRIMITIVE_TOPOLOGY to_primitive_topology(primitive_topology topology) {
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

		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE to_render_pass_beginning_access_type(pass_load_operation op) {
			constexpr static enum_mapping<pass_load_operation, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE> table{
				std::pair(pass_load_operation::discard,  D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD ),
				std::pair(pass_load_operation::preserve, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE),
				std::pair(pass_load_operation::clear,    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR   ),
			};
			return table[op];
		}

		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE to_render_pass_ending_access_type(pass_store_operation op) {
			constexpr static enum_mapping<pass_store_operation, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE> table{
				std::pair(pass_store_operation::discard,  D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD ),
				std::pair(pass_store_operation::preserve, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE),
			};
			return table[op];
		}

		D3D12_DESCRIPTOR_RANGE_TYPE to_descriptor_range_type(descriptor_type ty) {
			constexpr static enum_mapping<descriptor_type, D3D12_DESCRIPTOR_RANGE_TYPE> table{
				std::pair(descriptor_type::sampler,                D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER),
				std::pair(descriptor_type::read_only_image,        D3D12_DESCRIPTOR_RANGE_TYPE_SRV    ),
				std::pair(descriptor_type::read_write_image,       D3D12_DESCRIPTOR_RANGE_TYPE_UAV    ),
				std::pair(descriptor_type::read_only_buffer,       D3D12_DESCRIPTOR_RANGE_TYPE_SRV    ),
				std::pair(descriptor_type::read_write_buffer,      D3D12_DESCRIPTOR_RANGE_TYPE_UAV    ),
				std::pair(descriptor_type::constant_buffer,        D3D12_DESCRIPTOR_RANGE_TYPE_CBV    ),
				std::pair(descriptor_type::acceleration_structure, D3D12_DESCRIPTOR_RANGE_TYPE_SRV    ),
			};
			return table[ty];
		}

		D3D12_RESOURCE_STATES to_resource_states(image_usage st) {
			constexpr static enum_mapping<image_usage, D3D12_RESOURCE_STATES> table{
				std::pair(image_usage::color_render_target,         D3D12_RESOURCE_STATE_RENDER_TARGET                                                         ),
				std::pair(image_usage::depth_stencil_render_target, D3D12_RESOURCE_STATE_DEPTH_WRITE                                                           ),
				std::pair(image_usage::read_only_texture,           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				std::pair(image_usage::read_write_color_texture,    D3D12_RESOURCE_STATE_UNORDERED_ACCESS                                                      ),
				std::pair(image_usage::present,                     D3D12_RESOURCE_STATE_PRESENT                                                               ),
				std::pair(image_usage::copy_source,                 D3D12_RESOURCE_STATE_COPY_SOURCE                                                           ),
				std::pair(image_usage::copy_destination,            D3D12_RESOURCE_STATE_COPY_DEST                                                             ),
				std::pair(image_usage::initial,                     D3D12_RESOURCE_STATE_COMMON                                                                ),
			};
			return table[st];
		}

		D3D12_RESOURCE_STATES to_resource_states(buffer_usage st) {
			constexpr static enum_mapping<buffer_usage, D3D12_RESOURCE_STATES> table{
				std::pair(buffer_usage::index_buffer,           D3D12_RESOURCE_STATE_INDEX_BUFFER                                                                                                            ),
				std::pair(buffer_usage::vertex_buffer,          D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER                                                                                              ),
				std::pair(buffer_usage::read_only_buffer,       D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				std::pair(buffer_usage::read_write_buffer,      D3D12_RESOURCE_STATE_UNORDERED_ACCESS                                                                                                        ),
				std::pair(buffer_usage::copy_source,            D3D12_RESOURCE_STATE_COPY_SOURCE                                                                                                             ),
				std::pair(buffer_usage::copy_destination,       D3D12_RESOURCE_STATE_COPY_DEST                                                                                                               ),
				std::pair(buffer_usage::acceleration_structure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE                                                                                       ),
			};
			return table[st];
		}

		D3D12_HEAP_TYPE to_heap_type(heap_type ty) {
			constexpr static enum_mapping<heap_type, D3D12_HEAP_TYPE> table{
				std::pair(heap_type::device_only, D3D12_HEAP_TYPE_DEFAULT ),
				std::pair(heap_type::upload,      D3D12_HEAP_TYPE_UPLOAD  ),
				std::pair(heap_type::readback,    D3D12_HEAP_TYPE_READBACK),
			};
			return table[ty];
		}

		D3D12_TEXTURE_ADDRESS_MODE to_texture_address_mode(sampler_address_mode mode) {
			constexpr static enum_mapping<sampler_address_mode, D3D12_TEXTURE_ADDRESS_MODE> table{
				std::pair(sampler_address_mode::repeat, D3D12_TEXTURE_ADDRESS_MODE_WRAP  ),
				std::pair(sampler_address_mode::mirror, D3D12_TEXTURE_ADDRESS_MODE_MIRROR),
				std::pair(sampler_address_mode::clamp,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP ),
				std::pair(sampler_address_mode::border, D3D12_TEXTURE_ADDRESS_MODE_BORDER),
			};
			return table[mode];
		}

		D3D12_COMPARISON_FUNC to_comparison_function(comparison_function mode) {
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


		D3D12_COLOR_WRITE_ENABLE to_color_write_mask(channel_mask mask) {
			constexpr static std::pair<channel_mask, D3D12_COLOR_WRITE_ENABLE> table[]{
				{ channel_mask::red,   D3D12_COLOR_WRITE_ENABLE_RED   },
				{ channel_mask::green, D3D12_COLOR_WRITE_ENABLE_GREEN },
				{ channel_mask::blue,  D3D12_COLOR_WRITE_ENABLE_BLUE  },
				{ channel_mask::alpha, D3D12_COLOR_WRITE_ENABLE_ALPHA },
			};
			std::uint8_t result = 0;
			for (auto [myval, dxval] : table) {
				if ((mask & myval) == myval) {
					result |= dxval;
					mask &= ~myval;
				}
			}
			assert(mask == channel_mask::none); // make sure we've exhausted all bits
			return static_cast<D3D12_COLOR_WRITE_ENABLE>(result);
		}

		D3D12_SHADER_VISIBILITY to_shader_visibility(shader_stage stage) {
			constexpr static enum_mapping<shader_stage, D3D12_SHADER_VISIBILITY> table{
				std::pair(shader_stage::all,                   D3D12_SHADER_VISIBILITY_ALL     ),
				std::pair(shader_stage::vertex_shader,         D3D12_SHADER_VISIBILITY_VERTEX  ),
				std::pair(shader_stage::geometry_shader,       D3D12_SHADER_VISIBILITY_GEOMETRY),
				std::pair(shader_stage::pixel_shader,          D3D12_SHADER_VISIBILITY_PIXEL   ),
				std::pair(shader_stage::compute_shader,        D3D12_SHADER_VISIBILITY_ALL     ),
				std::pair(shader_stage::callable_shader,       D3D12_SHADER_VISIBILITY_ALL     ),
				std::pair(shader_stage::ray_generation_shader, D3D12_SHADER_VISIBILITY_ALL     ),
				std::pair(shader_stage::intersection_shader,   D3D12_SHADER_VISIBILITY_ALL     ),
				std::pair(shader_stage::any_hit_shader,        D3D12_SHADER_VISIBILITY_ALL     ),
				std::pair(shader_stage::closest_hit_shader,    D3D12_SHADER_VISIBILITY_ALL     ),
				std::pair(shader_stage::miss_shader,           D3D12_SHADER_VISIBILITY_ALL     ),
			};
			return table[stage];
		}


		D3D12_FILTER to_filter(
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


		D3D12_VIEWPORT to_viewport(const viewport &vp) {
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

		D3D12_RECT to_rect(const aab2i &rect) {
			D3D12_RECT result = {};
			result.left   = static_cast<LONG>(rect.min[0]);
			result.top    = static_cast<LONG>(rect.min[1]);
			result.right  = static_cast<LONG>(rect.max[0]);
			result.bottom = static_cast<LONG>(rect.max[1]);
			return result;
		}


		D3D12_RENDER_TARGET_BLEND_DESC to_render_target_blend_description(
			const render_target_blend_options &opt
		) {
			D3D12_RENDER_TARGET_BLEND_DESC result = {};
			result.BlendEnable           = opt.enabled;
			result.SrcBlend              = to_blend_factor(opt.source_color);
			result.DestBlend             = to_blend_factor(opt.destination_color);
			result.BlendOp               = to_blend_operation(opt.color_operation);
			result.SrcBlendAlpha         = to_blend_factor(opt.source_alpha);
			result.DestBlendAlpha        = to_blend_factor(opt.destination_alpha);
			result.BlendOp               = to_blend_operation(opt.color_operation);
			result.RenderTargetWriteMask = static_cast<UINT8>(to_color_write_mask(opt.write_mask));
			return result;
		}

		D3D12_BLEND_DESC to_blend_description(std::span<const render_target_blend_options> targets) {
			assert(targets.size() < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
			D3D12_BLEND_DESC result = {};
			// TODO handle logic operations
			result.IndependentBlendEnable = true;
			for (std::size_t i = 0; i < targets.size(); ++i) {
				result.RenderTarget[i] = to_render_target_blend_description(targets[i]);
			}
			return result;
		}

		D3D12_RASTERIZER_DESC to_rasterizer_description(const rasterizer_options &opt) {
			D3D12_RASTERIZER_DESC result = {};
			result.FillMode              = opt.is_wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
			result.CullMode              = to_cull_mode(opt.culling);
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

		D3D12_DEPTH_STENCILOP_DESC to_depth_stencil_operation_description(const stencil_options &op) {
			D3D12_DEPTH_STENCILOP_DESC result = {};
			result.StencilFailOp      = to_stencil_operation(op.fail);
			result.StencilDepthFailOp = to_stencil_operation(op.depth_fail);
			result.StencilPassOp      = to_stencil_operation(op.pass);
			result.StencilFunc        = to_comparison_function(op.comparison);
			return result;
		}

		D3D12_DEPTH_STENCIL_DESC to_depth_stencil_description(const depth_stencil_options &opt) {
			D3D12_DEPTH_STENCIL_DESC result = {};
			result.DepthEnable      = opt.enable_depth_testing;
			result.DepthWriteMask   = opt.write_depth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
			result.DepthFunc        = to_comparison_function(opt.depth_comparison);
			result.StencilEnable    = opt.enable_stencil_testing;
			result.StencilReadMask  = opt.stencil_read_mask;
			result.StencilWriteMask = opt.stencil_write_mask;
			result.FrontFace        = to_depth_stencil_operation_description(opt.stencil_front_face);
			result.BackFace         = to_depth_stencil_operation_description(opt.stencil_back_face);
			return result;
		}

		D3D12_RENDER_PASS_RENDER_TARGET_DESC to_render_pass_render_target_description(
			const render_target_pass_options &opt
		) {
			D3D12_RENDER_PASS_RENDER_TARGET_DESC result = {};
			result.BeginningAccess.Clear.ClearValue.Format = to_format(opt.pixel_format);
			result.BeginningAccess.Type                    = to_render_pass_beginning_access_type(opt.load_operation);
			result.EndingAccess.Type                       = to_render_pass_ending_access_type(opt.store_operation);
			return result;
		}

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC to_render_pass_depth_stencil_description(
			const depth_stencil_pass_options &opt
		) {
			D3D12_RENDER_PASS_DEPTH_STENCIL_DESC result = {};
			DXGI_FORMAT format = to_format(opt.pixel_format);
			if (format_properties::get(opt.pixel_format).depth_bits > 0) {
				result.DepthBeginningAccess.Clear.ClearValue.Format = format;
				result.DepthBeginningAccess.Type = to_render_pass_beginning_access_type(opt.depth_load_operation);
				result.DepthEndingAccess.Type    = to_render_pass_ending_access_type(opt.depth_store_operation);
			} else {
				result.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
				result.DepthEndingAccess.Type    = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			}
			if (format_properties::get(opt.pixel_format).stencil_bits > 0) {
				result.StencilBeginningAccess.Clear.ClearValue.Format = format;
				result.StencilBeginningAccess.Type =
					to_render_pass_beginning_access_type(opt.stencil_load_operation);
				result.StencilEndingAccess.Type    = to_render_pass_ending_access_type(opt.stencil_store_operation);
			} else {
				result.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
				result.StencilEndingAccess.Type    = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			}
			return result;
		}


		shader_resource_binding back_to_shader_resource_binding(const D3D12_SHADER_INPUT_BIND_DESC &desc) {
			shader_resource_binding result = uninitialized;
			result.first_register = desc.BindPoint;
			result.register_count = desc.BindCount;
			result.register_space = desc.Space;
			result.name           = reinterpret_cast<const char8_t*>(desc.Name);

			switch (desc.Type) {
			case D3D_SIT_CBUFFER:
				result.type = descriptor_type::constant_buffer;
				break;
			case D3D_SIT_TBUFFER:
				result.type = descriptor_type::read_write_image;
				break;
			case D3D_SIT_TEXTURE:
				result.type = descriptor_type::read_only_image;
				break;
			case D3D_SIT_SAMPLER:
				result.type = descriptor_type::sampler;
				break;
			case D3D_SIT_UAV_RWTYPED: [[fallthrough]];
			case D3D_SIT_UAV_RWSTRUCTURED:
				result.type = descriptor_type::read_write_buffer;
				break;
			case D3D_SIT_STRUCTURED:
				result.type = descriptor_type::read_only_buffer;
				break;
			default:
				assert(false); // not supported
			}

			return result;
		}

		shader_output_variable back_to_shader_output_variable(const D3D12_SIGNATURE_PARAMETER_DESC &desc) {
			shader_output_variable result = uninitialized;
			result.semantic_name  = reinterpret_cast<const char8_t*>(desc.SemanticName);
			result.semantic_index = desc.SemanticIndex;
			for (auto &ch : result.semantic_name) {
				ch = static_cast<char8_t>(std::toupper(ch));
			}
			return result;
		}
	}


	D3D12_HEAP_PROPERTIES heap_type_to_properties(heap_type type) {
		D3D12_HEAP_PROPERTIES result = {};
		result.Type                 = conversions::to_heap_type(type);
		result.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		result.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		result.CreationNodeMask     = 0;
		result.VisibleNodeMask      = 0;
		return result;
	}

	UINT compute_subresource_index(const subresource_index &index, ID3D12Resource *rsrc) {
		D3D12_RESOURCE_DESC desc = rsrc->GetDesc();
		return D3D12CalcSubresource(index.mip_level, index.array_slice, 0, desc.MipLevels, desc.DepthOrArraySize);
	}


	LPCWSTR shader_name(std::size_t index) {
		static std::deque<std::basic_string<WCHAR>> _names;
		static std::mutex _mutex;

		// TODO this is really bad
		std::lock_guard<std::mutex> lock(_mutex);
		if (_names.size() <= index) {
			for (std::size_t i = 0; i < std::max<std::size_t>(1024, index + 1); ++i) {
				auto &str = _names.emplace_back(L"_");
				for (std::size_t id = _names.size(); id > 0; id /= 10) {
					str.push_back(L'0' + id % 10);
				}
			}
		}
		return _names[index].c_str();
	}

	LPCWSTR shader_record_name(std::size_t index) {
		static std::deque<std::basic_string<WCHAR>> _names;
		static std::mutex _mutex;

		// TODO this is really bad
		std::lock_guard<std::mutex> lock(_mutex);
		if (_names.size() <= index) {
			for (std::size_t i = 0; i < std::max<std::size_t>(1024, index + 1); ++i) {
				auto &str = _names.emplace_back(L"_R");
				for (std::size_t id = _names.size(); id > 0; id /= 10) {
					str.push_back(L'0' + id % 10);
				}
			}
		}
		return _names[index].c_str();
	}


	namespace resource_desc {
		D3D12_RESOURCE_DESC for_buffer(std::size_t size) {
			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			desc.Width              = memory::align_size(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			desc.Height             = 1;
			desc.DepthOrArraySize   = 1;
			desc.MipLevels          = 1;
			desc.Format             = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			return desc;
		}

		void adjust_resource_flags_for_buffer(
			heap_type type, buffer_usage::mask all_usages,
			D3D12_RESOURCE_DESC &desc, D3D12_RESOURCE_STATES &states, D3D12_HEAP_FLAGS *heap_flags
		) {
			if (type == heap_type::device_only) {
				if (!is_empty(all_usages & buffer_usage::mask::acceleration_structure)) {
					assert(all_usages == buffer_usage::mask::acceleration_structure);
					states = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
				} else if (!is_empty(all_usages & buffer_usage::mask::read_write_buffer)) {
					desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
					if (heap_flags) {
						*heap_flags |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;	
					}
				}
			} else if (type == heap_type::upload) {
				states = D3D12_RESOURCE_STATE_GENERIC_READ;
				desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
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
			desc.Format             = conversions::to_format(fmt);
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout             = conversions::to_texture_layout(tiling);
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
