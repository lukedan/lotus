#pragma once

/// \file
/// Miscellaneous internal functions and classes used by the Vulkan backend.

#include <vulkan/vulkan.hpp>
#include <spirv_reflect.h>

#include "lotus/logging.h"
#include "lotus/graphics/common.h"

namespace lotus::graphics::backends::vulkan::_details {
	/// Asserts that the result is \p vk::Result::eSuccess.
	inline void assert_vk(vk::Result result) {
		if (result != vk::Result::eSuccess) {
			log().error<u8"Vulkan error {}">(vk::to_string(result));
			assert(false);
		}
	}
	/// Returns the result value if it's valid, and asserts otherwise.
	template <typename Val> inline Val unwrap(vk::ResultValue<Val> res) {
		assert_vk(res.result);
		return std::move(res.value);
	}
	/// Asserts that the given result is 
	inline void assert_spv_reflect(SpvReflectResult result) {
		if (result != SPV_REFLECT_RESULT_SUCCESS) {
			log().error<u8"SPIRV-Reflect error {}">(static_cast<std::underlying_type_t<decltype(result)>>(result));
			assert(false);
		}
	}

	/// Conversions to Vulkan data types.
	namespace conversions {
		/// Converts a \ref format to a \p vk::Format.
		[[nodiscard]] vk::Format to_format(format);
		/// Converts a \ref index_format to a \p vk::IndexType.
		[[nodiscard]] vk::IndexType to_index_type(index_format);
		/// Converts a \ref image_aspect_mask to a \p vk::ImageAspectFlags.
		[[nodiscard]] vk::ImageAspectFlags to_image_aspect_flags(image_aspect_mask);
		/// Converts a \ref image_usage to a \p vk::AccessFlags and \p vk::ImageLayout.
		[[nodiscard]] std::pair<vk::AccessFlags, vk::ImageLayout> to_image_access_layout(image_usage);
		/// Converts a \ref buffer_usage::mask to a \p vk::BufferUsageFlags.
		[[nodiscard]] vk::BufferUsageFlags to_buffer_usage_flags(buffer_usage::mask);
		/// Converts a \ref image_usage::mask to a \p vk::ImageUsageFlags.
		[[nodiscard]] vk::ImageUsageFlags to_image_usage_flags(image_usage::mask);
		/// Converts a \ref buffer_usage to a \p vk::AccessFlags.
		[[nodiscard]] vk::AccessFlags to_access_flags(buffer_usage);
		/// Converts a \ref descriptor_type to a \p vk::DescriptorType.
		[[nodiscard]] vk::DescriptorType to_descriptor_type(descriptor_type);
		/// Converts a \ref filtering to a \p vk::Filter.
		[[nodiscard]] vk::Filter to_filter(filtering);
		/// Converts a \ref filtering to a \p vk::SamplerMipmapMode.
		[[nodiscard]] vk::SamplerMipmapMode to_sampler_mipmap_mode(filtering);
		/// Converts a \ref sampler_address_mode to a \p vk::SamplerAddressMode.
		[[nodiscard]] vk::SamplerAddressMode to_sampler_address_mode(sampler_address_mode);
		/// Converts a \ref comparison_function to a \p vk::CompareOp.
		[[nodiscard]] vk::CompareOp to_compare_op(comparison_function);
		/// Converts a \ref shader_stage to a \p vk::ShaderStageFlags.
		[[nodiscard]] vk::ShaderStageFlags to_shader_stage_flags(shader_stage);
		/// Converts a \ref input_buffer_rate to a \p vk::VertexInputRate.
		[[nodiscard]] vk::VertexInputRate to_vertex_input_rate(input_buffer_rate);
		/// Converts a \ref primitive_topology to a \p vk::PrimitiveTopology.
		[[nodiscard]] vk::PrimitiveTopology to_primitive_topology(primitive_topology);
		/// Converts a \ref cull_mode to a \p vk::CullModeFlags.
		[[nodiscard]] vk::CullModeFlags to_cull_mode_flags(cull_mode);
		/// Converts a \ref front_facing_mode to a \p vk::FrontFace.
		[[nodiscard]] vk::FrontFace to_front_face(front_facing_mode);
		/// Converts a \ref stencil_operation to a \p vk::StencilOp.
		[[nodiscard]] vk::StencilOp to_stencil_op(stencil_operation);
		/// Converts a \ref blend_factor to a \p vk::BlendFactor.
		[[nodiscard]] vk::BlendFactor to_blend_factor(blend_factor);
		/// Converts a \ref blend_operation to a \p vk::BlendOp.
		[[nodiscard]] vk::BlendOp to_blend_op(blend_operation);
		/// Converts a \ref channel_mask to a \p vk::ColorComponentFlags.
		[[nodiscard]] vk::ColorComponentFlags to_color_component_flags(channel_mask);
		/// Converts a \ref pass_load_operation to a \p vk::AttachmentLoadOp.
		[[nodiscard]] vk::AttachmentLoadOp to_attachment_load_op(pass_load_operation);
		/// Converts a \ref pass_store_operation to a \p vk::AttachmentStoreOp.
		[[nodiscard]] vk::AttachmentStoreOp to_attachment_store_op(pass_store_operation);
		/// Converts a \ref image_tiling to a \p vk::ImageTiling.
		[[nodiscard]] vk::ImageTiling to_image_tiling(image_tiling);

		/// Converts a vector to a \p vk::Offset2D.
		template <typename Int> [[nodiscard]] inline vk::Offset2D to_offset_2d(cvec2<Int> off) {
			return vk::Offset2D(static_cast<std::int32_t>(off[0]), static_cast<std::int32_t>(off[1]));
		}
		/// Converts a vector to a \p vk::Extent2D.
		template <typename Int> [[nodiscard]] inline vk::Extent2D to_extent_2d(cvec2<Int> ext) {
			return vk::Extent2D(static_cast<std::uint32_t>(ext[0]), static_cast<std::uint32_t>(ext[1]));
		}

		/// Converts a \ref subresource_index to a \p vk::ImageSubresourceLayers.
		[[nodiscard]] vk::ImageSubresourceLayers to_image_subresource_layers(const subresource_index&);
		/// Converts a \ref subresource_index to a \p vk::ImageSubresourceRange.
		[[nodiscard]] vk::ImageSubresourceRange to_image_subresource_range(const subresource_index&);
		/// Converts a \ref mip_levels to a \p vk::ImageSubresourceRange.
		[[nodiscard]] vk::ImageSubresourceRange to_image_subresource_range(const mip_levels&, vk::ImageAspectFlags);
		/// Converts a \ref subresource_index to a \p vk::ImageSubresource.
		[[nodiscard]] vk::ImageSubresource to_image_subresource(const subresource_index&);
		/// Converts a \ref stencil_options to a \p vk::StencilOpState.
		[[nodiscard]] vk::StencilOpState to_stencil_op_state(
			const stencil_options&, std::uint32_t cmp_mask, std::uint32_t write_mask
		);
		/// Converts a \ref color_clear_value to a \p vk::ClearValue.
		[[nodiscard]] vk::ClearValue to_clear_value(const color_clear_value&);


		/// Converts a \p vk::Format back to a \ref format.
		[[nodiscard]] format back_to_format(vk::Format);
		/// Converts a \p vk::MemoryPropertyFlagBits to a \ref memory_properties.
		[[nodiscard]] memory_properties back_to_memory_properties(vk::MemoryPropertyFlags);

		/// Converts a \p SpvReflectDescriptorBinding back to a \ref shader_resource_binding.
		[[nodiscard]] shader_resource_binding back_to_shader_resource_binding(const SpvReflectDescriptorBinding&);
		/// Converts a \p SpvReflectInterfaceVariable back to a \ref shader_output_variable.
		[[nodiscard]] shader_output_variable back_to_shader_output_variable(const SpvReflectInterfaceVariable&);
	}
}
