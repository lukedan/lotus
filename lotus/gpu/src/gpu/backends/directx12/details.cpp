#include "lotus/gpu/backends/directx12/details.h"

/// \file
/// Implementation of miscellaneous DirectX 12 related functions.

#include <mutex>

#include <comdef.h>
#include <directx/d3dx12.h>

#include "lotus/memory/common.h"
#include "lotus/system/platforms/windows/details.h"
#include "lotus/gpu/backends/common/dxgi_format.h"
#include "lotus/gpu/backends/directx12/device.h"

namespace lotus::gpu::backends::directx12::_details {
	void assert_dx(HRESULT hr, ID3D12Device *dev) {
		if (FAILED(hr)) {
			_com_error err(hr);
			auto msg = system::platforms::windows::_details::tstring_to_u8string(
				err.ErrorMessage(), std::allocator<char8_t>()
			);
			log().error("DirectX error {:X}: {}", hr, string::to_generic(msg));

			if (dev && hr == DXGI_ERROR_DEVICE_REMOVED) {
				assert_dx(dev->GetDeviceRemovedReason());
			}

			std::abort();
		}
	}


	namespace conversions {
		DXGI_FORMAT to_format(format fmt) {
			return backends::common::_details::conversions::to_format(fmt);
		}

		DXGI_FORMAT to_format(index_format fmt) {
			constexpr static enums::sequential_mapping<index_format, DXGI_FORMAT> table{
				std::pair(index_format::uint16, DXGI_FORMAT_R16_UINT),
				std::pair(index_format::uint32, DXGI_FORMAT_R32_UINT),
			};
			return table[fmt];
		}

		D3D12_TEXTURE_LAYOUT to_texture_layout(image_tiling tiling) {
			constexpr static enums::sequential_mapping<image_tiling, D3D12_TEXTURE_LAYOUT> table{
				std::pair(image_tiling::row_major, D3D12_TEXTURE_LAYOUT_ROW_MAJOR),
				std::pair(image_tiling::optimal,   D3D12_TEXTURE_LAYOUT_UNKNOWN  ),
			};
			return table[tiling];
		}

		D3D12_BLEND to_blend_factor(blend_factor factor) {
			constexpr static enums::sequential_mapping<blend_factor, D3D12_BLEND> table{
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
			constexpr static enums::sequential_mapping<blend_operation, D3D12_BLEND_OP> table{
				std::pair(blend_operation::add,              D3D12_BLEND_OP_ADD         ),
				std::pair(blend_operation::subtract,         D3D12_BLEND_OP_SUBTRACT    ),
				std::pair(blend_operation::reverse_subtract, D3D12_BLEND_OP_REV_SUBTRACT),
				std::pair(blend_operation::min,              D3D12_BLEND_OP_MIN         ),
				std::pair(blend_operation::max,              D3D12_BLEND_OP_MAX         ),
			};
			return table[op];
		}

		D3D12_CULL_MODE to_cull_mode(cull_mode mode) {
			constexpr static enums::sequential_mapping<cull_mode, D3D12_CULL_MODE> table{
				std::pair(cull_mode::none,       D3D12_CULL_MODE_NONE ),
				std::pair(cull_mode::cull_front, D3D12_CULL_MODE_FRONT),
				std::pair(cull_mode::cull_back,  D3D12_CULL_MODE_BACK ),
			};
			return table[mode];
		}

		D3D12_STENCIL_OP to_stencil_operation(stencil_operation op) {
			constexpr static enums::sequential_mapping<stencil_operation, D3D12_STENCIL_OP> table{
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
			constexpr static enums::sequential_mapping<input_buffer_rate, D3D12_INPUT_CLASSIFICATION> table{
				std::pair(input_buffer_rate::per_vertex,   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA  ),
				std::pair(input_buffer_rate::per_instance, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA),
			};
			return table[rate];
		}

		D3D12_PRIMITIVE_TOPOLOGY_TYPE to_primitive_topology_type(primitive_topology topology) {
			constexpr static enums::sequential_mapping<primitive_topology, D3D12_PRIMITIVE_TOPOLOGY_TYPE> table{
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
			constexpr static enums::sequential_mapping<primitive_topology, D3D_PRIMITIVE_TOPOLOGY> table{
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
			constexpr static enums::sequential_mapping<
				pass_load_operation, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE
			> table{
				std::pair(pass_load_operation::discard,  D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD ),
				std::pair(pass_load_operation::preserve, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE),
				std::pair(pass_load_operation::clear,    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR   ),
			};
			return table[op];
		}

		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE to_render_pass_ending_access_type(pass_store_operation op) {
			constexpr static enums::sequential_mapping<
				pass_store_operation, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE
			> table{
				std::pair(pass_store_operation::discard,  D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD ),
				std::pair(pass_store_operation::preserve, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE),
			};
			return table[op];
		}

		D3D12_DESCRIPTOR_RANGE_TYPE to_descriptor_range_type(descriptor_type ty) {
			constexpr static enums::sequential_mapping<descriptor_type, D3D12_DESCRIPTOR_RANGE_TYPE> table{
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

		D3D12_BARRIER_SYNC to_barrier_sync(synchronization_point_mask mask) {
			constexpr static bit_mask::mapping<synchronization_point_mask, D3D12_BARRIER_SYNC> table{
				std::pair(synchronization_point_mask::all,                          D3D12_BARRIER_SYNC_ALL                                    ),
				std::pair(synchronization_point_mask::all_graphics,                 D3D12_BARRIER_SYNC_DRAW                                   ),
				std::pair(synchronization_point_mask::index_input,                  D3D12_BARRIER_SYNC_INPUT_ASSEMBLER                        ),
				std::pair(synchronization_point_mask::vertex_input,                 D3D12_BARRIER_SYNC_VERTEX_SHADING                         ),
				std::pair(synchronization_point_mask::vertex_shader,                D3D12_BARRIER_SYNC_VERTEX_SHADING                         ),
				std::pair(synchronization_point_mask::pixel_shader,                 D3D12_BARRIER_SYNC_PIXEL_SHADING                          ),
				std::pair(synchronization_point_mask::depth_stencil_read_write,     D3D12_BARRIER_SYNC_DEPTH_STENCIL                          ),
				std::pair(synchronization_point_mask::render_target_read_write,     D3D12_BARRIER_SYNC_RENDER_TARGET                          ),
				std::pair(synchronization_point_mask::compute_shader,               D3D12_BARRIER_SYNC_COMPUTE_SHADING                        ),
				std::pair(synchronization_point_mask::raytracing,                   D3D12_BARRIER_SYNC_RAYTRACING                             ),
				std::pair(synchronization_point_mask::copy,                         D3D12_BARRIER_SYNC_COPY                                   ),
				std::pair(synchronization_point_mask::acceleration_structure_build, D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE),
				std::pair(synchronization_point_mask::acceleration_structure_copy,  D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE ),
				std::pair(synchronization_point_mask::cpu_access,                   D3D12_BARRIER_SYNC_NONE                                   ),
			};
			return table.get_union(mask);
		}

		D3D12_RESOURCE_FLAGS to_resource_flags(image_usage_mask mask) {
			constexpr static bit_mask::mapping<image_usage_mask, D3D12_RESOURCE_FLAGS> table{
				std::pair(image_usage_mask::copy_source,                 D3D12_RESOURCE_FLAG_NONE                  ),
				std::pair(image_usage_mask::copy_destination,            D3D12_RESOURCE_FLAG_NONE                  ),
				std::pair(image_usage_mask::shader_read,                 D3D12_RESOURCE_FLAG_NONE                  ),
				std::pair(image_usage_mask::shader_write,                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				std::pair(image_usage_mask::color_render_target,         D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET   ),
				std::pair(image_usage_mask::depth_stencil_render_target, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL   ),
			};
			return table.get_union(mask);
		}

		D3D12_RESOURCE_FLAGS to_resource_flags(buffer_usage_mask mask) {
			constexpr static bit_mask::mapping<buffer_usage_mask, D3D12_RESOURCE_FLAGS> table{
				std::pair(buffer_usage_mask::copy_source,                        D3D12_RESOURCE_FLAG_NONE                             ),
				std::pair(buffer_usage_mask::copy_destination,                   D3D12_RESOURCE_FLAG_NONE                             ),
				std::pair(buffer_usage_mask::shader_read,                        D3D12_RESOURCE_FLAG_NONE                             ),
				std::pair(buffer_usage_mask::shader_write,                       D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS           ),
				std::pair(buffer_usage_mask::index_buffer,                       D3D12_RESOURCE_FLAG_NONE                             ),
				std::pair(buffer_usage_mask::vertex_buffer,                      D3D12_RESOURCE_FLAG_NONE                             ),
				std::pair(buffer_usage_mask::acceleration_structure_build_input, D3D12_RESOURCE_FLAG_NONE                             ),
				std::pair(buffer_usage_mask::acceleration_structure,             D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE),
				std::pair(buffer_usage_mask::shader_record_table,                D3D12_RESOURCE_FLAG_NONE                             ),
			};
			return table.get_union(mask);
		}

		D3D12_BARRIER_ACCESS to_barrier_access(image_access_mask mask) {
			constexpr static bit_mask::mapping<image_access_mask, D3D12_BARRIER_ACCESS> table{
				std::pair(image_access_mask::copy_source,              D3D12_BARRIER_ACCESS_COPY_SOURCE        ),
				std::pair(image_access_mask::copy_destination,         D3D12_BARRIER_ACCESS_COPY_DEST          ),
				std::pair(image_access_mask::color_render_target,      D3D12_BARRIER_ACCESS_RENDER_TARGET      ),
				std::pair(image_access_mask::depth_stencil_read_only,  D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ ),
				std::pair(image_access_mask::depth_stencil_read_write, D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE),
				std::pair(image_access_mask::shader_read,              D3D12_BARRIER_ACCESS_SHADER_RESOURCE    ),
				std::pair(image_access_mask::shader_write,             D3D12_BARRIER_ACCESS_UNORDERED_ACCESS   ),
			};
			if (mask == image_access_mask::none) {
				return D3D12_BARRIER_ACCESS_NO_ACCESS; // not zero!
			}
			if (bit_mask::contains<image_access_mask::shader_write>(mask)) {
				mask &= ~image_access_mask::shader_read;
			}
			return table.get_union(mask);
		}

		D3D12_BARRIER_ACCESS to_barrier_access(buffer_access_mask mask) {
			constexpr static bit_mask::mapping<buffer_access_mask, D3D12_BARRIER_ACCESS> table{
				std::pair(buffer_access_mask::copy_source,                        D3D12_BARRIER_ACCESS_COPY_SOURCE                            ),
				std::pair(buffer_access_mask::copy_destination,                   D3D12_BARRIER_ACCESS_COPY_DEST                              ),
				std::pair(buffer_access_mask::vertex_buffer,                      D3D12_BARRIER_ACCESS_VERTEX_BUFFER                          ),
				std::pair(buffer_access_mask::index_buffer,                       D3D12_BARRIER_ACCESS_INDEX_BUFFER                           ),
				std::pair(buffer_access_mask::constant_buffer,                    D3D12_BARRIER_ACCESS_CONSTANT_BUFFER                        ),
				std::pair(buffer_access_mask::shader_read,                        D3D12_BARRIER_ACCESS_SHADER_RESOURCE                        ),
				std::pair(buffer_access_mask::shader_write,                       D3D12_BARRIER_ACCESS_UNORDERED_ACCESS                       ),
				std::pair(buffer_access_mask::acceleration_structure_build_input, D3D12_BARRIER_ACCESS_SHADER_RESOURCE                        ),
				std::pair(buffer_access_mask::acceleration_structure_read,        D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ ),
				std::pair(buffer_access_mask::acceleration_structure_write,       D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE),
				std::pair(buffer_access_mask::cpu_read,                           D3D12_BARRIER_ACCESS_COMMON                                 ),
				std::pair(buffer_access_mask::cpu_write,                          D3D12_BARRIER_ACCESS_COMMON                                 ),
			};
			mask &= ~(buffer_access_mask::cpu_read | buffer_access_mask::cpu_write);
			if (mask == buffer_access_mask::none) {
				return D3D12_BARRIER_ACCESS_NO_ACCESS; // not zero!
			}
			return table.get_union(mask);
		}

		D3D12_BARRIER_LAYOUT to_barrier_layout(image_layout layout) {
			constexpr static enums::sequential_mapping<image_layout, D3D12_BARRIER_LAYOUT> table{
				// use COMMON as initial state so that resources can be used on copy queues from the get go
				std::pair(image_layout::undefined,                D3D12_BARRIER_LAYOUT_COMMON             ),
				std::pair(image_layout::general,                  D3D12_BARRIER_LAYOUT_COMMON             ),
				std::pair(image_layout::copy_source,              D3D12_BARRIER_LAYOUT_COPY_SOURCE        ),
				std::pair(image_layout::copy_destination,         D3D12_BARRIER_LAYOUT_COPY_DEST          ),
				std::pair(image_layout::present,                  D3D12_BARRIER_LAYOUT_PRESENT            ),
				std::pair(image_layout::color_render_target,      D3D12_BARRIER_LAYOUT_RENDER_TARGET      ),
				std::pair(image_layout::depth_stencil_read_only,  D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ ),
				std::pair(image_layout::depth_stencil_read_write, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE),
				std::pair(image_layout::shader_read_only,         D3D12_BARRIER_LAYOUT_SHADER_RESOURCE    ),
				std::pair(image_layout::shader_read_write,        D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS   ),
			};
			return table[layout];
		}

		D3D12_TEXTURE_ADDRESS_MODE to_texture_address_mode(sampler_address_mode mode) {
			constexpr static enums::sequential_mapping<sampler_address_mode, D3D12_TEXTURE_ADDRESS_MODE> table{
				std::pair(sampler_address_mode::repeat, D3D12_TEXTURE_ADDRESS_MODE_WRAP  ),
				std::pair(sampler_address_mode::mirror, D3D12_TEXTURE_ADDRESS_MODE_MIRROR),
				std::pair(sampler_address_mode::clamp,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP ),
				std::pair(sampler_address_mode::border, D3D12_TEXTURE_ADDRESS_MODE_BORDER),
			};
			return table[mode];
		}

		D3D12_COMPARISON_FUNC to_comparison_function(comparison_function mode) {
			constexpr static enums::sequential_mapping<comparison_function, D3D12_COMPARISON_FUNC> table{
				std::pair(comparison_function::none,             D3D12_COMPARISON_FUNC_NONE         ),
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

		D3D12_SHADER_VERSION_TYPE to_shader_version_type(shader_stage stage) {
			constexpr static enums::sequential_mapping<shader_stage, D3D12_SHADER_VERSION_TYPE> table{
				std::pair(shader_stage::all,                   D3D12_SHVER_LIBRARY              ),
				std::pair(shader_stage::vertex_shader,         D3D12_SHVER_VERTEX_SHADER        ),
				std::pair(shader_stage::geometry_shader,       D3D12_SHVER_GEOMETRY_SHADER      ),
				std::pair(shader_stage::pixel_shader,          D3D12_SHVER_PIXEL_SHADER         ),
				std::pair(shader_stage::compute_shader,        D3D12_SHVER_COMPUTE_SHADER       ),
				std::pair(shader_stage::callable_shader,       D3D12_SHVER_CALLABLE_SHADER      ),
				std::pair(shader_stage::ray_generation_shader, D3D12_SHVER_RAY_GENERATION_SHADER),
				std::pair(shader_stage::intersection_shader,   D3D12_SHVER_INTERSECTION_SHADER  ),
				std::pair(shader_stage::any_hit_shader,        D3D12_SHVER_ANY_HIT_SHADER       ),
				std::pair(shader_stage::closest_hit_shader,    D3D12_SHVER_CLOSEST_HIT_SHADER   ),
				std::pair(shader_stage::miss_shader,           D3D12_SHVER_MISS_SHADER          ),
			};
			return table[stage];
		}

		D3D12_COMMAND_LIST_TYPE to_command_list_type(queue_family f) {
			constexpr static enums::sequential_mapping<queue_family, D3D12_COMMAND_LIST_TYPE> table{
				std::pair(queue_family::graphics, D3D12_COMMAND_LIST_TYPE_DIRECT ),
				std::pair(queue_family::compute,  D3D12_COMMAND_LIST_TYPE_COMPUTE),
				std::pair(queue_family::copy,     D3D12_COMMAND_LIST_TYPE_COPY   ),
			};
			return table[f];
		}

		D3D12_RAYTRACING_INSTANCE_FLAGS to_raytracing_instance_flags(raytracing_instance_flags flags) {
			constexpr static bit_mask::mapping<
				raytracing_instance_flags, D3D12_RAYTRACING_INSTANCE_FLAGS
			> table{
				std::pair(raytracing_instance_flags::disable_triangle_culling,        D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE          ),
				std::pair(raytracing_instance_flags::triangle_front_counterclockwise, D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE),
				std::pair(raytracing_instance_flags::force_opaque,                    D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE                   ),
				std::pair(raytracing_instance_flags::force_non_opaque,                D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE               ),
			};
			return table.get_union(flags);
		}

		D3D12_RAYTRACING_GEOMETRY_FLAGS to_raytracing_geometry_flags(raytracing_geometry_flags flags) {
			constexpr static bit_mask::mapping<
				raytracing_geometry_flags, D3D12_RAYTRACING_GEOMETRY_FLAGS
			> table{
				std::pair(raytracing_geometry_flags::opaque,                          D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE                        ),
				std::pair(raytracing_geometry_flags::no_duplicate_any_hit_invocation, D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION),
			};
			return table.get_union(flags);
		}


		D3D12_COLOR_WRITE_ENABLE to_color_write_mask(channel_mask mask) {
			constexpr static bit_mask::mapping<channel_mask, D3D12_COLOR_WRITE_ENABLE> table{
				std::pair(channel_mask::red,   D3D12_COLOR_WRITE_ENABLE_RED  ),
				std::pair(channel_mask::green, D3D12_COLOR_WRITE_ENABLE_GREEN),
				std::pair(channel_mask::blue,  D3D12_COLOR_WRITE_ENABLE_BLUE ),
				std::pair(channel_mask::alpha, D3D12_COLOR_WRITE_ENABLE_ALPHA),
			};
			return table.get_union(mask);
		}

		D3D12_SHADER_VISIBILITY to_shader_visibility(shader_stage stage) {
			constexpr static enums::sequential_mapping<shader_stage, D3D12_SHADER_VISIBILITY> table{
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
			result.BlendOpAlpha          = to_blend_operation(opt.alpha_operation);
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

		D3D12_BARRIER_SUBRESOURCE_RANGE to_barrier_subresource_range(const subresource_range &r) {
			D3D12_BARRIER_SUBRESOURCE_RANGE range = {};
			range.IndexOrFirstMipLevel = r.mips.first_level;
			range.NumMipLevels         = r.mips.get_num_levels_as<UINT>().value_or(-1); // TODO: is this supported? not explicitly in the docs
			range.FirstArraySlice      = r.first_array_slice;
			range.NumArraySlices       = r.num_array_slices;
			if (bit_mask::contains<image_aspect_mask::color>(r.aspects)) {
				assert(r.aspects == image_aspect_mask::color);
				range.FirstPlane = 0;
				range.NumPlanes  = 1;
			} else {
				if (r.aspects == image_aspect_mask::depth) {
					range.FirstPlane = 0;
					range.NumPlanes  = 1;
				} else if (r.aspects == (image_aspect_mask::depth | image_aspect_mask::stencil)) {
					range.FirstPlane = 0;
					range.NumPlanes  = 2;
				} else {
					std::abort(); // not implemented
				}
			}
			return range;
		}


		debug_message_severity back_to_debug_message_severity(D3D12_MESSAGE_SEVERITY severity) {
			constexpr static enums::sequential_mapping<
				D3D12_MESSAGE_SEVERITY, debug_message_severity,
				static_cast<std::size_t>(D3D12_MESSAGE_SEVERITY_MESSAGE) + 1
			> table{
				std::pair(D3D12_MESSAGE_SEVERITY_CORRUPTION, debug_message_severity::error),
				std::pair(D3D12_MESSAGE_SEVERITY_ERROR,      debug_message_severity::error),
				std::pair(D3D12_MESSAGE_SEVERITY_WARNING,    debug_message_severity::warning),
				std::pair(D3D12_MESSAGE_SEVERITY_INFO,       debug_message_severity::information),
				std::pair(D3D12_MESSAGE_SEVERITY_MESSAGE,    debug_message_severity::debug),
			};
			return table[severity];
		}

		shader_resource_binding back_to_shader_resource_binding(const D3D12_SHADER_INPUT_BIND_DESC &desc) {
			shader_resource_binding result = uninitialized;
			result.first_register = desc.BindPoint;
			result.register_count =
				desc.BindCount == 0 ?
				gpu::descriptor_range::unbounded_count :
				desc.BindCount;
			result.register_space = desc.Space;
			result.name           = reinterpret_cast<const char8_t*>(desc.Name);

			switch (desc.Type) {
			case D3D_SIT_CBUFFER:
				result.type = descriptor_type::constant_buffer;
				break;
			case D3D_SIT_TBUFFER:
				result.type = descriptor_type::read_only_buffer;
				break;
			case D3D_SIT_TEXTURE:
				result.type = descriptor_type::read_only_image;
				break;
			case D3D_SIT_UAV_RWTYPED:
				if (desc.Dimension == D3D_SRV_DIMENSION_BUFFER || desc.Dimension == D3D_SRV_DIMENSION_BUFFEREX) {
					result.type = descriptor_type::read_write_buffer;
				} else {
					result.type = descriptor_type::read_write_image;
				}
				break;
			case D3D_SIT_SAMPLER:
				result.type = descriptor_type::sampler;
				break;
			case D3D_SIT_STRUCTURED:
				result.type = descriptor_type::read_only_buffer;
				break;
			case D3D_SIT_UAV_RWSTRUCTURED:
				result.type = descriptor_type::read_write_buffer;
				break;
			case D3D_SIT_RTACCELERATIONSTRUCTURE:
				result.type = descriptor_type::acceleration_structure;
				break;
			default:
				crash_if(true); // not supported
				break;
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


	D3D12_HEAP_PROPERTIES default_heap_properties(D3D12_HEAP_TYPE type) {
		D3D12_HEAP_PROPERTIES result = {};
		result.Type = type;
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
					str.push_back(L'0' + (id % 10));
				}
			}
		}
		return _names[index].c_str();
	}


	namespace resource_desc {
		D3D12_RESOURCE_DESC1 for_buffer(std::size_t size, buffer_usage_mask usages) {
			D3D12_RESOURCE_DESC1 desc = {};
			desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			desc.Width              = memory::align_up(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			desc.Height             = 1;
			desc.DepthOrArraySize   = 1;
			desc.MipLevels          = 1;
			desc.Format             = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags              = conversions::to_resource_flags(usages);
			return desc;
		}

		void adjust_resource_flags_for_buffer(
			D3D12_HEAP_TYPE type, buffer_usage_mask usages, D3D12_HEAP_FLAGS *heap_flags
		) {
			if (heap_flags && type == D3D12_HEAP_TYPE_DEFAULT) {
				if (bit_mask::contains<buffer_usage_mask::shader_write>(usages)) {
					*heap_flags |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;	
				}
			}
		}

		D3D12_RESOURCE_DESC1 for_image2d(
			cvec2u32 size, std::uint32_t mip_levels, format fmt, image_tiling tiling, image_usage_mask all_usages
		) {
			D3D12_RESOURCE_DESC1 desc = {};
			desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			desc.Width              = static_cast<UINT64>(size[0]);
			desc.Height             = static_cast<UINT>(size[1]);
			desc.DepthOrArraySize   = 1;
			desc.MipLevels          = static_cast<UINT16>(mip_levels);
			desc.Format             = conversions::to_format(fmt);
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout             = conversions::to_texture_layout(tiling);
			desc.Flags              = conversions::to_resource_flags(all_usages);
			return desc;
		}

		D3D12_RESOURCE_DESC1 for_image3d(
			cvec3u32 size, std::uint32_t mip_levels, format fmt, image_tiling tiling, image_usage_mask all_usages
		) {
			D3D12_RESOURCE_DESC1 desc = {};
			desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			desc.Width              = static_cast<UINT64>(size[0]);
			desc.Height             = static_cast<UINT>(size[1]);
			crash_if(size[2] > std::numeric_limits<UINT16>::max());
			desc.DepthOrArraySize   = static_cast<UINT16>(size[2]);
			desc.MipLevels          = static_cast<UINT16>(mip_levels);
			desc.Format             = conversions::to_format(fmt);
			desc.SampleDesc.Count   = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout             = conversions::to_texture_layout(tiling);
			desc.Flags              = conversions::to_resource_flags(all_usages);
			return desc;
		}

		void adjust_resource_flags_for_image(
			format, image_usage_mask all_usages, D3D12_HEAP_FLAGS *heap_flags
		) {
			if (heap_flags) {
				if (bit_mask::contains<image_usage_mask::shader_write>(all_usages)) {
					*heap_flags |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
				}
			}
		}
	}
}
