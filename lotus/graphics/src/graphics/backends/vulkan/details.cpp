#include "lotus/graphics/backends/vulkan/details.h"

/// \file
/// Implementation of miscellaneous internal functions.

#include "lotus/common.h"

namespace lotus::graphics::backends::vulkan::_details {
	namespace conversions {
		constexpr static enum_mapping<format, vk::Format> _format_table{
			std::pair(format::none,               vk::Format::eUndefined         ),
			std::pair(format::d32_float_s8,       vk::Format::eD32SfloatS8Uint   ),
			std::pair(format::d32_float,          vk::Format::eD32Sfloat         ),
			std::pair(format::d24_unorm_s8,       vk::Format::eD24UnormS8Uint    ),
			std::pair(format::d16_unorm,          vk::Format::eD16Unorm          ),
			std::pair(format::r8_unorm,           vk::Format::eR8Unorm           ),
			std::pair(format::r8_snorm,           vk::Format::eR8Snorm           ),
			std::pair(format::r8_uint,            vk::Format::eR8Uint            ),
			std::pair(format::r8_sint,            vk::Format::eR8Sint            ),
			std::pair(format::r8g8_unorm,         vk::Format::eR8G8Unorm         ),
			std::pair(format::r8g8_snorm,         vk::Format::eR8G8Snorm         ),
			std::pair(format::r8g8_uint,          vk::Format::eR8G8Uint          ),
			std::pair(format::r8g8_sint,          vk::Format::eR8G8Sint          ),
			std::pair(format::r8g8b8a8_unorm,     vk::Format::eR8G8B8A8Unorm     ),
			std::pair(format::r8g8b8a8_snorm,     vk::Format::eR8G8B8A8Snorm     ),
			std::pair(format::r8g8b8a8_srgb,      vk::Format::eR8G8B8A8Srgb      ),
			std::pair(format::r8g8b8a8_uint,      vk::Format::eR8G8B8A8Uint      ),
			std::pair(format::r8g8b8a8_sint,      vk::Format::eR8G8B8A8Sint      ),
			std::pair(format::b8g8r8a8_unorm,     vk::Format::eB8G8R8A8Unorm     ),
			std::pair(format::b8g8r8a8_srgb,      vk::Format::eB8G8R8A8Srgb      ),
			std::pair(format::r16_unorm,          vk::Format::eR16Unorm          ),
			std::pair(format::r16_snorm,          vk::Format::eR16Snorm          ),
			std::pair(format::r16_uint,           vk::Format::eR16Uint           ),
			std::pair(format::r16_sint,           vk::Format::eR16Sint           ),
			std::pair(format::r16_float,          vk::Format::eR16Sfloat         ),
			std::pair(format::r16g16_unorm,       vk::Format::eR16G16Unorm       ),
			std::pair(format::r16g16_snorm,       vk::Format::eR16G16Snorm       ),
			std::pair(format::r16g16_uint,        vk::Format::eR16G16Uint        ),
			std::pair(format::r16g16_sint,        vk::Format::eR16G16Sint        ),
			std::pair(format::r16g16_float,       vk::Format::eR16G16Sfloat      ),
			std::pair(format::r16g16b16a16_unorm, vk::Format::eR16G16B16A16Unorm ),
			std::pair(format::r16g16b16a16_snorm, vk::Format::eR16G16B16A16Snorm ),
			std::pair(format::r16g16b16a16_uint,  vk::Format::eR16G16B16A16Uint  ),
			std::pair(format::r16g16b16a16_sint,  vk::Format::eR16G16B16A16Sint  ),
			std::pair(format::r16g16b16a16_float, vk::Format::eR16G16B16A16Sfloat),
			std::pair(format::r32_uint,           vk::Format::eR32Uint           ),
			std::pair(format::r32_sint,           vk::Format::eR32Sint           ),
			std::pair(format::r32_float,          vk::Format::eR32Sfloat         ),
			std::pair(format::r32g32_uint,        vk::Format::eR32G32Uint        ),
			std::pair(format::r32g32_sint,        vk::Format::eR32G32Sint        ),
			std::pair(format::r32g32_float,       vk::Format::eR32G32Sfloat      ),
			std::pair(format::r32g32b32_uint,     vk::Format::eR32G32B32Uint     ),
			std::pair(format::r32g32b32_sint,     vk::Format::eR32G32B32Sint     ),
			std::pair(format::r32g32b32_float,    vk::Format::eR32G32B32Sfloat   ),
			std::pair(format::r32g32b32a32_uint,  vk::Format::eR32G32B32A32Uint  ),
			std::pair(format::r32g32b32a32_sint,  vk::Format::eR32G32B32A32Sint  ),
			std::pair(format::r32g32b32a32_float, vk::Format::eR32G32B32A32Sfloat),
		};
		vk::Format for_format(format fmt) {
			return _format_table[fmt];
		}

		vk::IndexType for_index_format(index_format fmt) {
			constexpr static enum_mapping<index_format, vk::IndexType> table{
				std::pair(index_format::uint16, vk::IndexType::eUint16),
				std::pair(index_format::uint32, vk::IndexType::eUint32),
			};
			return table[fmt];
		}

		vk::ImageAspectFlags for_image_aspect_mask(image_aspect_mask mask) {
			constexpr static std::pair<image_aspect_mask, vk::ImageAspectFlagBits> table[]{
				{ image_aspect_mask::color,   vk::ImageAspectFlagBits::eColor   },
				{ image_aspect_mask::depth,   vk::ImageAspectFlagBits::eDepth   },
				{ image_aspect_mask::stencil, vk::ImageAspectFlagBits::eStencil },
			};
			vk::ImageAspectFlags result;
			for (auto [myval, vkval] : table) {
				if ((mask & myval) == myval) {
					result |= vkval;
					mask &= ~myval;
				}
			}
			assert(is_empty(mask));
			return result;
		}

		std::pair<vk::AccessFlags, vk::ImageLayout> to_image_access_layout(image_usage usage) {
			constexpr static enum_mapping<image_usage, std::pair<vk::AccessFlags, vk::ImageLayout>> table{
				std::pair(image_usage::color_render_target,         std::pair(vk::AccessFlagBits::eColorAttachmentWrite,                          vk::ImageLayout::eColorAttachmentOptimal       )),
				std::pair(image_usage::depth_stencil_render_target, std::pair(vk::AccessFlagBits::eDepthStencilAttachmentWrite,                   vk::ImageLayout::eDepthStencilAttachmentOptimal)),
				std::pair(image_usage::read_only_texture,           std::pair(vk::AccessFlagBits::eShaderRead,                                    vk::ImageLayout::eShaderReadOnlyOptimal        )),
				std::pair(image_usage::read_write_color_texture,    std::pair(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::ImageLayout::eGeneral                      )),
				std::pair(image_usage::present,                     std::pair(vk::AccessFlags(),                                                  vk::ImageLayout::ePresentSrcKHR                )),
				std::pair(image_usage::copy_source,                 std::pair(vk::AccessFlagBits::eTransferRead,                                  vk::ImageLayout::eTransferSrcOptimal           )),
				std::pair(image_usage::copy_destination,            std::pair(vk::AccessFlagBits::eTransferWrite,                                 vk::ImageLayout::eTransferDstOptimal           )),
				std::pair(image_usage::initial,                     std::pair(vk::AccessFlags(),                                                  vk::ImageLayout::eUndefined                    )),
			};
			return table[usage];
		}

		vk::BufferUsageFlags to_buffer_usage_flags(buffer_usage::mask mask) {
			constexpr static std::pair<buffer_usage::mask, vk::BufferUsageFlags> table[]{
				{ buffer_usage::mask::index_buffer,           vk::BufferUsageFlagBits::eIndexBuffer                                             },
				{ buffer_usage::mask::vertex_buffer,          vk::BufferUsageFlagBits::eVertexBuffer                                            },
				{ buffer_usage::mask::read_only_buffer,       vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer },
				{ buffer_usage::mask::read_write_buffer,      vk::BufferUsageFlagBits::eStorageBuffer                                           },
				{ buffer_usage::mask::copy_source,            vk::BufferUsageFlagBits::eTransferSrc                                             },
				{ buffer_usage::mask::copy_destination,       vk::BufferUsageFlagBits::eTransferDst                                             },
				{ buffer_usage::mask::acceleration_structure, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR                         },
			};
			vk::BufferUsageFlags result;
			for (auto [myval, vkval] : table) {
				if ((mask & myval) == myval) {
					result |= vkval;
					mask &= ~myval;
				}
			}
			assert(is_empty(mask));
			return result;
		}

		vk::ImageUsageFlags to_image_usage_flags(image_usage::mask mask) {
			constexpr static std::pair<image_usage::mask, vk::ImageUsageFlags> table[]{
				{ image_usage::mask::color_render_target,         vk::ImageUsageFlagBits::eColorAttachment        },
				{ image_usage::mask::depth_stencil_render_target, vk::ImageUsageFlagBits::eDepthStencilAttachment },
				{ image_usage::mask::read_only_texture,           vk::ImageUsageFlagBits::eSampled                },
				{ image_usage::mask::read_write_color_texture,    vk::ImageUsageFlagBits::eStorage                },
				{ image_usage::mask::present,                     vk::ImageUsageFlagBits()                        }, // TODO what should this be?
				{ image_usage::mask::copy_source,                 vk::ImageUsageFlagBits::eTransferSrc            },
				{ image_usage::mask::copy_destination,            vk::ImageUsageFlagBits::eTransferDst            },
			};
			vk::ImageUsageFlags result;
			for (auto [myval, vkval] : table) {
				if ((mask & myval) == myval) {
					result |= vkval;
					mask &= ~myval;
				}
			}
			assert(is_empty(mask));
			return result;
		}

		vk::AccessFlags to_access_flags(buffer_usage usage) {
			constexpr static enum_mapping<buffer_usage, vk::AccessFlags> table{
				std::pair(buffer_usage::index_buffer,           vk::AccessFlagBits::eIndexRead                                    ),
				std::pair(buffer_usage::vertex_buffer,          vk::AccessFlagBits::eVertexAttributeRead                          ),
				std::pair(buffer_usage::read_only_buffer,       vk::AccessFlagBits::eUniformRead                                  ),
				std::pair(buffer_usage::read_write_buffer,      vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				std::pair(buffer_usage::copy_source,            vk::AccessFlagBits::eTransferRead                                 ),
				std::pair(buffer_usage::copy_destination,       vk::AccessFlagBits::eTransferWrite                                ),
				std::pair(buffer_usage::acceleration_structure, vk::AccessFlagBits::eAccelerationStructureReadKHR                 ), // TODO this won't work
			};
			return table[usage];
		}

		vk::DescriptorType to_descriptor_type(descriptor_type ty) {
			constexpr static enum_mapping<descriptor_type, vk::DescriptorType> table{
				std::pair(descriptor_type::sampler,                vk::DescriptorType::eSampler                 ),
				std::pair(descriptor_type::read_only_image,        vk::DescriptorType::eSampledImage            ),
				std::pair(descriptor_type::read_write_image,       vk::DescriptorType::eStorageImage            ),
				std::pair(descriptor_type::read_only_buffer,       vk::DescriptorType::eStorageBuffer           ), // aka structured buffer
				std::pair(descriptor_type::read_write_buffer,      vk::DescriptorType::eStorageBuffer           ),
				std::pair(descriptor_type::constant_buffer,        vk::DescriptorType::eUniformBuffer           ),
				std::pair(descriptor_type::acceleration_structure, vk::DescriptorType::eAccelerationStructureKHR),
			};
			return table[ty];
		}

		vk::Filter to_filter(filtering f) {
			constexpr static enum_mapping<filtering, vk::Filter> table{
				std::pair(filtering::nearest, vk::Filter::eNearest),
				std::pair(filtering::linear,  vk::Filter::eLinear ),
			};
			return table[f];
		}

		vk::SamplerMipmapMode to_sampler_mipmap_mode(filtering f) {
			constexpr static enum_mapping<filtering, vk::SamplerMipmapMode> table{
				std::pair(filtering::nearest, vk::SamplerMipmapMode::eNearest),
				std::pair(filtering::linear,  vk::SamplerMipmapMode::eLinear ),
			};
			return table[f];
		}

		vk::SamplerAddressMode to_sampler_address_mode(sampler_address_mode m) {
			constexpr static enum_mapping<sampler_address_mode, vk::SamplerAddressMode> table{
				std::pair(sampler_address_mode::repeat, vk::SamplerAddressMode::eRepeat        ),
				std::pair(sampler_address_mode::mirror, vk::SamplerAddressMode::eMirroredRepeat),
				std::pair(sampler_address_mode::clamp,  vk::SamplerAddressMode::eClampToEdge   ),
				std::pair(sampler_address_mode::border, vk::SamplerAddressMode::eClampToBorder ),
			};
			return table[m];
		}

		vk::CompareOp to_compare_op(comparison_function m) {
			constexpr static enum_mapping<comparison_function, vk::CompareOp> table{
				std::pair(comparison_function::never,            vk::CompareOp::eNever         ),
				std::pair(comparison_function::less,             vk::CompareOp::eLess          ),
				std::pair(comparison_function::equal,            vk::CompareOp::eEqual         ),
				std::pair(comparison_function::less_or_equal,    vk::CompareOp::eLessOrEqual   ),
				std::pair(comparison_function::greater,          vk::CompareOp::eGreater       ),
				std::pair(comparison_function::not_equal,        vk::CompareOp::eNotEqual      ),
				std::pair(comparison_function::greater_or_equal, vk::CompareOp::eGreaterOrEqual),
				std::pair(comparison_function::always,           vk::CompareOp::eAlways        ),
			};
			return table[m];
		}

		vk::ShaderStageFlags to_shader_stage_flags(shader_stage stage) {
			constexpr static enum_mapping<shader_stage, vk::ShaderStageFlagBits> table{
				std::pair(shader_stage::all,                   vk::ShaderStageFlagBits::eAll            ),
				std::pair(shader_stage::vertex_shader,         vk::ShaderStageFlagBits::eVertex         ),
				std::pair(shader_stage::geometry_shader,       vk::ShaderStageFlagBits::eGeometry       ),
				std::pair(shader_stage::pixel_shader,          vk::ShaderStageFlagBits::eFragment       ),
				std::pair(shader_stage::compute_shader,        vk::ShaderStageFlagBits::eCompute        ),
				std::pair(shader_stage::callable_shader,       vk::ShaderStageFlagBits::eCallableKHR    ),
				std::pair(shader_stage::ray_generation_shader, vk::ShaderStageFlagBits::eRaygenKHR      ),
				std::pair(shader_stage::intersection_shader,   vk::ShaderStageFlagBits::eIntersectionKHR),
				std::pair(shader_stage::any_hit_shader,        vk::ShaderStageFlagBits::eAnyHitKHR      ),
				std::pair(shader_stage::closest_hit_shader,    vk::ShaderStageFlagBits::eClosestHitKHR  ),
				std::pair(shader_stage::miss_shader,           vk::ShaderStageFlagBits::eMissKHR        ),
			};
			return table[stage];
		}

		vk::VertexInputRate to_vertex_input_rate(input_buffer_rate rate) {
			constexpr static enum_mapping<input_buffer_rate, vk::VertexInputRate> table{
				std::pair(input_buffer_rate::per_vertex,   vk::VertexInputRate::eVertex  ),
				std::pair(input_buffer_rate::per_instance, vk::VertexInputRate::eInstance),
			};
			return table[rate];
		}

		vk::PrimitiveTopology to_primitive_topology(primitive_topology t) {
			constexpr static enum_mapping<primitive_topology, vk::PrimitiveTopology> table{
				std::pair(primitive_topology::point_list,                    vk::PrimitiveTopology::ePointList                 ),
				std::pair(primitive_topology::line_list,                     vk::PrimitiveTopology::eLineList                  ),
				std::pair(primitive_topology::line_strip,                    vk::PrimitiveTopology::eLineStrip                 ),
				std::pair(primitive_topology::triangle_list,                 vk::PrimitiveTopology::eTriangleList              ),
				std::pair(primitive_topology::triangle_strip,                vk::PrimitiveTopology::eTriangleStrip             ),
				std::pair(primitive_topology::line_list_with_adjacency,      vk::PrimitiveTopology::eLineListWithAdjacency     ),
				std::pair(primitive_topology::line_strip_with_adjacency,     vk::PrimitiveTopology::eLineStripWithAdjacency    ),
				std::pair(primitive_topology::triangle_list_with_adjacency,  vk::PrimitiveTopology::eTriangleListWithAdjacency ),
				std::pair(primitive_topology::triangle_strip_with_adjacency, vk::PrimitiveTopology::eTriangleStripWithAdjacency),
			};
			return table[t];
		}

		vk::CullModeFlags to_cull_mode_flags(cull_mode mode) {
			constexpr static enum_mapping<cull_mode, vk::CullModeFlags> table{
				std::pair(cull_mode::none,       vk::CullModeFlagBits::eNone ),
				std::pair(cull_mode::cull_front, vk::CullModeFlagBits::eFront),
				std::pair(cull_mode::cull_back,  vk::CullModeFlagBits::eBack ),
			};
			return table[mode];
		}

		vk::FrontFace to_front_face(front_facing_mode mode) {
			constexpr static enum_mapping<front_facing_mode, vk::FrontFace> table{
				std::pair(front_facing_mode::clockwise,         vk::FrontFace::eClockwise       ),
				std::pair(front_facing_mode::counter_clockwise, vk::FrontFace::eCounterClockwise),
			};
			return table[mode];
		}

		vk::StencilOp to_stencil_op(stencil_operation op) {
			constexpr static enum_mapping<stencil_operation, vk::StencilOp> table{
				std::pair(stencil_operation::keep,                vk::StencilOp::eKeep             ),
				std::pair(stencil_operation::zero,                vk::StencilOp::eZero             ),
				std::pair(stencil_operation::replace,             vk::StencilOp::eReplace          ),
				std::pair(stencil_operation::increment_and_clamp, vk::StencilOp::eIncrementAndClamp),
				std::pair(stencil_operation::decrement_and_clamp, vk::StencilOp::eDecrementAndClamp),
				std::pair(stencil_operation::bitwise_invert,      vk::StencilOp::eInvert           ),
				std::pair(stencil_operation::increment_and_wrap,  vk::StencilOp::eIncrementAndWrap ),
				std::pair(stencil_operation::decrement_and_wrap,  vk::StencilOp::eDecrementAndWrap ),
			};
			return table[op];
		}

		vk::BlendFactor to_blend_factor(blend_factor f) {
			constexpr static enum_mapping<blend_factor, vk::BlendFactor> table{
				std::pair(blend_factor::zero,                        vk::BlendFactor::eZero            ),
				std::pair(blend_factor::one,                         vk::BlendFactor::eOne             ),
				std::pair(blend_factor::source_color,                vk::BlendFactor::eSrcColor        ),
				std::pair(blend_factor::one_minus_source_color,      vk::BlendFactor::eOneMinusSrcColor),
				std::pair(blend_factor::destination_color,           vk::BlendFactor::eDstColor        ),
				std::pair(blend_factor::one_minus_destination_color, vk::BlendFactor::eOneMinusDstColor),
				std::pair(blend_factor::source_alpha,                vk::BlendFactor::eSrcAlpha        ),
				std::pair(blend_factor::one_minus_source_alpha,      vk::BlendFactor::eOneMinusSrcAlpha),
				std::pair(blend_factor::destination_alpha,           vk::BlendFactor::eDstAlpha        ),
				std::pair(blend_factor::one_minus_destination_alpha, vk::BlendFactor::eOneMinusDstAlpha),
			};
			return table[f];
		}

		vk::BlendOp to_blend_op(blend_operation op) {
			constexpr static enum_mapping<blend_operation, vk::BlendOp> table{
				std::pair(blend_operation::add,              vk::BlendOp::eAdd            ),
				std::pair(blend_operation::subtract,         vk::BlendOp::eSubtract       ),
				std::pair(blend_operation::reverse_subtract, vk::BlendOp::eReverseSubtract),
				std::pair(blend_operation::min,              vk::BlendOp::eMin            ),
				std::pair(blend_operation::max,              vk::BlendOp::eMax            ),
			};
			return table[op];
		}

		vk::ColorComponentFlags to_color_component_flags(channel_mask mask) {
			constexpr static std::pair<channel_mask, vk::ColorComponentFlagBits> table[]{
				{ channel_mask::red,   vk::ColorComponentFlagBits::eR },
				{ channel_mask::green, vk::ColorComponentFlagBits::eG },
				{ channel_mask::blue,  vk::ColorComponentFlagBits::eB },
				{ channel_mask::alpha, vk::ColorComponentFlagBits::eA },
			};
			vk::ColorComponentFlags result;
			for (auto [myval, vkval] : table) {
				if ((mask & myval) == myval) {
					result |= vkval;
					mask &= ~myval;
				}
			}
			assert(is_empty(mask));
			return result;
		}

		vk::AttachmentLoadOp to_attachment_load_op(pass_load_operation op) {
			constexpr static enum_mapping<pass_load_operation, vk::AttachmentLoadOp> table{
				std::pair(pass_load_operation::discard,  vk::AttachmentLoadOp::eDontCare),
				std::pair(pass_load_operation::preserve, vk::AttachmentLoadOp::eLoad    ),
				std::pair(pass_load_operation::clear,    vk::AttachmentLoadOp::eClear   ),
			};
			return table[op];
		}

		vk::AttachmentStoreOp to_attachment_store_op(pass_store_operation op) {
			constexpr static enum_mapping<pass_store_operation, vk::AttachmentStoreOp> table{
				std::pair(pass_store_operation::discard,  vk::AttachmentStoreOp::eDontCare),
				std::pair(pass_store_operation::preserve, vk::AttachmentStoreOp::eStore   ),
			};
			return table[op];
		}

		vk::ImageTiling to_image_tiling(image_tiling t) {
			constexpr static enum_mapping<image_tiling, vk::ImageTiling> table{
				std::pair(image_tiling::row_major, vk::ImageTiling::eLinear ),
				std::pair(image_tiling::optimal,   vk::ImageTiling::eOptimal),
			};
			return table[t];
		}


		vk::ImageSubresourceLayers to_image_subresource_layers(const subresource_index &i) {
			vk::ImageSubresourceLayers result;
			result
				.setAspectMask(for_image_aspect_mask(i.aspects))
				.setMipLevel(i.mip_level)
				.setBaseArrayLayer(i.array_slice)
				.setLayerCount(1);
			return result;
		}

		vk::ImageSubresourceRange to_image_subresource_range(const subresource_index &id) {
			vk::ImageSubresourceRange result;
			result
				.setAspectMask(for_image_aspect_mask(id.aspects))
				.setBaseMipLevel(id.mip_level)
				.setLevelCount(1)
				.setBaseArrayLayer(id.array_slice)
				.setLayerCount(1);
			return result;
		}

		vk::ImageSubresourceRange to_image_subresource_range(const mip_levels &mip, vk::ImageAspectFlags aspects) {
			vk::ImageSubresourceRange result;
			result
				.setAspectMask(aspects)
				.setBaseMipLevel(mip.minimum)
				.setLevelCount(
					mip.num_levels == mip_levels::all_mip_levels ? VK_REMAINING_MIP_LEVELS : mip.num_levels
				)
				.setBaseArrayLayer(0)
				.setLayerCount(1);
			return result;
		}

		vk::ImageSubresource to_image_subresource(const subresource_index &i) {
			vk::ImageSubresource result;
			result
				.setAspectMask(for_image_aspect_mask(i.aspects))
				.setMipLevel(i.mip_level)
				.setArrayLayer(i.array_slice);
			return result;
		}

		vk::StencilOpState to_stencil_op_state(
			const stencil_options &op, std::uint32_t cmp_mask, std::uint32_t write_mask
		) {
			vk::StencilOpState result;
			result
				.setFailOp(to_stencil_op(op.fail))
				.setPassOp(to_stencil_op(op.pass))
				.setDepthFailOp(to_stencil_op(op.depth_fail))
				.setCompareOp(to_compare_op(op.comparison))
				.setCompareMask(cmp_mask)
				.setWriteMask(write_mask);
			return result;
		}

		/// Implementation for \ref to_clear_value() for \ref cvec4d.
		[[nodiscard]] static vk::ClearValue _to_clear_value_impl(const cvec4d &vec) {
			vk::ClearValue result;
			result.setColor(std::array<float, 4>{
				static_cast<float>(vec[0]),
				static_cast<float>(vec[1]),
				static_cast<float>(vec[2]),
				static_cast<float>(vec[3]),
			});
			return result;
		}
		/// Implementation for \ref to_clear_value() for \ref cvec4<std::uint64_t>.
		[[nodiscard]] static vk::ClearValue _to_clear_value_impl(const cvec4<std::uint64_t> &vec) {
			vk::ClearValue result;
			result.setColor(std::array<std::uint32_t, 4>{
				static_cast<std::uint32_t>(vec[0]),
				static_cast<std::uint32_t>(vec[1]),
				static_cast<std::uint32_t>(vec[2]),
				static_cast<std::uint32_t>(vec[3]),
			});
			return result;
		}
		vk::ClearValue to_clear_value(const color_clear_value &val) {
			return std::visit([](const auto &x) {
				return _to_clear_value_impl(x);
			}, val.value);
		}


		[[nodiscard]] constexpr static std::array<
			std::pair<format, vk::Format>, static_cast<std::size_t>(format::num_enumerators)
		> _get_sorted_format_table() {
			std::array result = _format_table.get_raw_table();
			std::sort(result.begin(), result.end(), [](const auto &lhs, const auto &rhs) constexpr {
				return lhs.second < rhs.second;
			});
			return result;
		}
		format back_to_format(vk::Format fmt) {
			constexpr static std::array table = _get_sorted_format_table();

			auto it = std::lower_bound(table.begin(), table.end(), fmt, [](const auto &pair, auto fmt) {
				return pair.second < fmt;
			});
			if (it != table.end() && it->second == fmt) {
				return it->first;
			}
			assert(false); // no format found
			return format::none;
		}

		shader_resource_binding back_to_shader_resource_binding(const SpvReflectDescriptorBinding &binding) {
			shader_resource_binding result = uninitialized;
			result.first_register = binding.binding;
			result.register_count = binding.count;
			result.register_space = binding.set;
			result.name           = reinterpret_cast<const char8_t*>(binding.name);

			switch (binding.descriptor_type) {
			case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
				result.type = descriptor_type::sampler;
				break;
			case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				result.type = descriptor_type::read_only_image;
				break;
			case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				result.type = descriptor_type::read_write_image;
				break;
			case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				result.type = descriptor_type::read_only_buffer;
				break;
			case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				result.type = descriptor_type::read_write_buffer;
				break;
			default:
				assert(false); // not supported
			}
			return result;
		}

		shader_output_variable back_to_shader_output_variable(const SpvReflectInterfaceVariable &var) {
			shader_output_variable result = uninitialized;
			result.semantic_name = reinterpret_cast<const char8_t*>(var.semantic);
			result.semantic_index = 0;
			std::size_t mul = 1;
			while (
				!result.semantic_name.empty() &&
				result.semantic_name.back() >= u8'0' &&
				result.semantic_name.back() <= u8'9'
			) {
				result.semantic_index += (result.semantic_name.back() - u8'0') * mul;
				mul *= 10;
				result.semantic_name.pop_back();
			}
			for (auto &ch : result.semantic_name) {
				ch = static_cast<char8_t>(std::toupper(ch));
			}
			return result;
		}
	}
}
