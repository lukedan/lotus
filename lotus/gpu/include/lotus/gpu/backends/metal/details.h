#pragma once

/// \file
/// Implementation details and helpers for the Metal backend.

#include <cstddef>
#include <utility>

#include <Metal/Metal.hpp>

#include "lotus/utils/static_function.h"
#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::metal::_details {
	// TODO
	/// Metal does not seem to have custom debug callbacks.
	using debug_message_id = int;
	/// Debug callback type for the Metal backend.
	using debug_message_callback =
		static_function<void(debug_message_severity, debug_message_id, std::u8string_view)>;


	/// Memory types supported by Metal.
	enum class memory_type_index : std::underlying_type_t<gpu::memory_type_index> {
		shared_cpu_cached,   ///< \p MTL::StorageModeShared with CPU-side caching enabled.
		shared_cpu_uncached, ///< \p MTL::StorageModeShared with CPU-side caching disabled.
		device_private,      ///< \p MTL::StroageModePrivate.

		num_enumerators ///< The number of enumerators.
	};

	/// Conversion helpers between lotus and Metal data types.
	namespace conversions {
		/// Converts a \ref format to a \p MTL::PixelFormat.
		[[nodiscard]] MTL::PixelFormat to_pixel_format(format);
		/// Converts a \ref format to a \p MTL::VertexFormat.
		[[nodiscard]] MTL::VertexFormat to_vertex_format(format);
		/// Converts a \ref memory_type_index to a \p MTL::ResourceOptions.
		[[nodiscard]] MTL::ResourceOptions to_resource_options(_details::memory_type_index);
		/// \overload
		[[nodiscard]] inline MTL::ResourceOptions to_resource_options(gpu::memory_type_index i) {
			return to_resource_options(static_cast<memory_type_index>(i));
		}
		/// Converts a \ref mip_levels to a \p NS::Range based on an associated texture.
		[[nodiscard]] NS::Range to_range(mip_levels, MTL::Texture*);
		/// Converts a \ref image_usage_mask to a \p MTL::TextureUsage.
		[[nodiscard]] MTL::TextureUsage to_texture_usage(image_usage_mask);
		/// Converts a \ref sampler_address_mode to a \p MTL::SamplerAddressMode.
		[[nodiscard]] MTL::SamplerAddressMode to_sampler_address_mode(sampler_address_mode);
		/// Converts a \ref filtering to a \p MTL::SamplerMinMagFilter.
		[[nodiscard]] MTL::SamplerMinMagFilter to_sampler_min_mag_filter(filtering);
		/// Converts a \ref filtering to a \p MTL::SamplerMipFilter.
		[[nodiscard]] MTL::SamplerMipFilter to_sampler_mip_filter(filtering);
		/// Converts a \ref comparison_function to a \p MTL::CompareFunction.
		[[nodiscard]] MTL::CompareFunction to_compare_function(comparison_function);
		/// Converts a \ref pass_load_operation to a \p MTL::LoadAction.
		[[nodiscard]] MTL::LoadAction to_load_action(pass_load_operation);
		/// Converts a \ref pass_store_operation to a \p MTL::StoreAction.
		[[nodiscard]] MTL::StoreAction to_store_action(pass_store_operation);
		/// Converts a \ref primitive_topology to a \p MTL::PrimitiveType.
		[[nodiscard]] MTL::PrimitiveType to_primitive_type(primitive_topology);
		/// Converts a \ref primitive_topology to a \p MTL::PrimitiveTopologyClass.
		[[nodiscard]] MTL::PrimitiveTopologyClass to_primitive_topology_class(primitive_topology);
		/// Converts a \ref index_format to a \p MTL::IndexType.
		[[nodiscard]] MTL::IndexType to_index_type(index_format);
		/// Converts a \ref input_buffer_rate to a \p MTL::VertexStepFunction.
		[[nodiscard]] MTL::VertexStepFunction to_vertex_step_function(input_buffer_rate);
		/// Converts a \ref front_facing_mode to a \p MTL::Winding.
		[[nodiscard]] MTL::Winding to_winding(front_facing_mode);
		/// Converts a \ref cull_mode to a \p MTL::CullMode.
		[[nodiscard]] MTL::CullMode to_cull_mode(cull_mode);
		/// Converts a \ref stencil_operation to a \p MTL::StencilOperation.
		[[nodiscard]] MTL::StencilOperation to_stencil_operation(stencil_operation);
		/// Converts a \ref blend_operation to a \p MTL::BlendOperation.
		[[nodiscard]] MTL::BlendOperation to_blend_operation(blend_operation);
		/// Converts a \ref blend_factor to a \p MTL::BlendFactor.
		[[nodiscard]] MTL::BlendFactor to_blend_factor(blend_factor);
		/// Converts a \ref channel_mask to a \p MTL::ColorWriteMask.
		[[nodiscard]] MTL::ColorWriteMask to_color_write_mask(channel_mask);
		/// Converts a C-style string to a \p NS::String.
		[[nodiscard]] NS::SharedPtr<NS::String> to_string(const char8_t*);
		/// Converts a \ref stencil_options to a \p MTL::StencilDescriptor.
		[[nodiscard]] NS::SharedPtr<MTL::StencilDescriptor> to_stencil_descriptor(
			stencil_options, std::uint8_t stencil_read, std::uint8_t stencil_write
		);

		/// Converts a \p NS::String back to a \p std::string.
		[[nodiscard]] std::u8string back_to_string(NS::String*);
		/// Converts a \p MTL::SizeAndAlign back to a \ref memory::size_alignment.
		[[nodiscard]] memory::size_alignment back_to_size_alignment(MTL::SizeAndAlign);
	}

	/// Creates a new \p MTL::TextureDescriptor based on the given settings.
	[[nodiscard]] NS::SharedPtr<MTL::TextureDescriptor> create_texture_descriptor(
		MTL::TextureType,
		format,
		cvec3u32 size,
		std::uint32_t mip_levels,
		MTL::ResourceOptions,
		image_usage_mask
	);
}
