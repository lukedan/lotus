#pragma once

/// \file
/// The Metal device.

#include <vector>
#include <span>

#include "lotus/gpu/common.h"
#include "details.h"
#include "acceleration_structure.h"
#include "commands.h"
#include "descriptors.h"
#include "frame_buffer.h"
#include "resources.h"
#include "synchronization.h"

namespace lotus::gpu::backends::metal {
	class adapter;
	class context;

	/// Holds a \p MTL::Device.
	class device {
		friend adapter;
		friend context;
	protected:
		/// Initializes this object to empty.
		device(std::nullptr_t) {
		}

		/// Calls \p CA::MetalLayer::nextDrawable().
		[[nodiscard]] back_buffer_info acquire_back_buffer(swap_chain&);
		/// Calls \p CA::MetalLayer::setDrawableSize().
		void resize_swap_chain_buffers(swap_chain&, cvec2u32 size);

		/// Returns a command allocator that corresponds to the given queue.
		[[nodiscard]] command_allocator create_command_allocator(command_queue&);
		/// Calls \p MTL::CommandQueue::commandBuffer().
		[[nodiscard]] command_list create_and_start_command_list(command_allocator&);

		/// Creates a new \p MTL::Heap that is used to allocate descriptor sets out of.
		[[nodiscard]] descriptor_pool create_descriptor_pool(
			std::span<const descriptor_range> capacity, std::size_t max_num_sets
		);
		/// Creates a new \p MTL::Buffer allocated out of the given \p MTL::Heap to be used as an argument buffer.
		[[nodiscard]] descriptor_set create_descriptor_set(descriptor_pool&, const descriptor_set_layout&);
		/// Creates an argument buffer for the given bindless descriptor layout.
		[[nodiscard]] descriptor_set create_descriptor_set(
			descriptor_pool&, const descriptor_set_layout&, std::size_t dynamic_size
		);

		/// Writes the given images into the descriptor table.
		void write_descriptor_set_read_only_images(
			descriptor_set&,
			const descriptor_set_layout&,
			std::size_t first_register,
			std::span<const image_view_base *const>
		);
		/// Writes the given images into the descriptor table.
		void write_descriptor_set_read_write_images(
			descriptor_set&,
			const descriptor_set_layout&,
			std::size_t first_register,
			std::span<const image_view_base *const>
		);

		/// Writes the given buffers into the descriptor table.
		void write_descriptor_set_read_only_structured_buffers(
			descriptor_set&,
			const descriptor_set_layout&,
			std::size_t first_register,
			std::span<const structured_buffer_view>
		);
		/// Writes the given buffers into the descriptor table.
		void write_descriptor_set_read_write_structured_buffers(
			descriptor_set&,
			const descriptor_set_layout&,
			std::size_t first_register,
			std::span<const structured_buffer_view>
		);

		/// Writes the given constant buffer into the descriptor table.
		void write_descriptor_set_constant_buffers(
			descriptor_set&,
			const descriptor_set_layout&,
			std::size_t first_register,
			std::span<const constant_buffer_view>
		);
		/// Writes the given sampler into the descriptor table.
		void write_descriptor_set_samplers(
			descriptor_set&,
			const descriptor_set_layout&,
			std::size_t first_register,
			std::span<const gpu::sampler *const>
		);

		/// Converts the input DXIL into Metal IR, then calls \p MTL::Device::newLibrary() to load the given shader
		/// blob.
		[[nodiscard]] shader_binary load_shader(std::span<const std::byte> data);

		/// Calls \p MTL::Device::newSamplerState().
		[[nodiscard]] sampler create_sampler(
			filtering minification, filtering magnification, filtering mipmapping,
			float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
			sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
			linear_rgba_f border_color, comparison_function comparison
		);

		/// Creates a new \ref descriptor_set_layout object.
		[[nodiscard]] descriptor_set_layout create_descriptor_set_layout(
			std::span<const descriptor_range_binding>, shader_stage
		);

		[[nodiscard]] pipeline_resources create_pipeline_resources(std::span<const gpu::descriptor_set_layout *const>); // TODO
		/// Creates a new \p MTL::RenderPipelineState and a new \p MTL::DepthStencilState.
		[[nodiscard]] graphics_pipeline_state create_graphics_pipeline_state(
			const pipeline_resources&,
			const shader_binary *vs,
			const shader_binary *ps,
			const shader_binary *ds,
			const shader_binary *hs,
			const shader_binary *gs,
			std::span<const render_target_blend_options>,
			const rasterizer_options&,
			const depth_stencil_options&,
			std::span<const input_buffer_layout>,
			primitive_topology,
			const frame_buffer_layout&,
			std::size_t num_viewports
		);
		[[nodiscard]] compute_pipeline_state create_compute_pipeline_state(
			const pipeline_resources&, const shader_binary&
		); // TODO

		/// Returns predefined memory types supported by Metal.
		[[nodiscard]] std::span<const std::pair<memory_type_index, memory_properties>> enumerate_memory_types() const;
		/// Calls \p MTL::Device::newHeap().
		[[nodiscard]] memory_block allocate_memory(std::size_t size, memory_type_index);
		/// Calls \p MTL::Device::newBuffer().
		[[nodiscard]] buffer create_committed_buffer(std::size_t size, memory_type_index, buffer_usage_mask);
		/// Calls \p MTL::Device::newTexture().
		[[nodiscard]] image2d create_committed_image2d(
			cvec2u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask
		);
		/// Calls \p MTL::Device::newTexture().
		[[nodiscard]] image3d create_committed_image3d(
			cvec3u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask
		);
		/// Staging buffers are tightly packed.
		[[nodiscard]] std::tuple<buffer, staging_buffer_metadata, std::size_t> create_committed_staging_buffer(
			cvec2u32 size, format, memory_type_index, buffer_usage_mask
		);

		/// Calls \p MTL::Device::heapTextureSizeAndAlign().
		[[nodiscard]] memory::size_alignment get_image2d_memory_requirements(
			cvec2u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask
		);
		/// Calls \p MTL::Device::heapTextureSizeAndAlign().
		[[nodiscard]] memory::size_alignment get_image3d_memory_requirements(
			cvec3u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask
		);
		/// Calls \p MTL::Device::heapBufferSizeAndAlign().
		[[nodiscard]] memory::size_alignment get_buffer_memory_requirements(std::size_t size, buffer_usage_mask);
		/// Calls \p MTL::Heap::newBuffer().
		[[nodiscard]] buffer create_placed_buffer(
			std::size_t size, buffer_usage_mask, const memory_block&, std::size_t offset
		);
		/// Calls \p MTL::Heap::newTexture().
		[[nodiscard]] image2d create_placed_image2d(
			cvec2u32 size,
			std::uint32_t mip_levels,
			format,
			image_tiling,
			image_usage_mask,
			const memory_block&,
			std::size_t offset
		);
		/// Calls \p MTL::Heap::newTexture().
		[[nodiscard]] image3d create_placed_image3d(
			cvec3u32 size,
			std::uint32_t mip_levels,
			format,
			image_tiling,
			image_usage_mask,
			const memory_block&,
			std::size_t offset
		);

		/// \return \p MTL::Buffer::contents().
		[[nodiscard]] std::byte *map_buffer(buffer&);
		/// Does nothing.
		void unmap_buffer(buffer&);
		/// Does nothing.
		void flush_mapped_buffer_to_host(buffer&, std::size_t begin, std::size_t length);
		/// Does nothing - MTL::Buffer::didModifyRange() is only needed for managed buffers.
		void flush_mapped_buffer_to_device(buffer&, std::size_t begin, std::size_t length);

		/// Creates a \ref image2d_view using \p MTL::Texture::newTextureView().
		[[nodiscard]] image2d_view create_image2d_view_from(const image2d&, format, mip_levels);
		/// Creates a \ref image3d_view using \p MTL::Texture::newTextureView().
		[[nodiscard]] image3d_view create_image3d_view_from(const image3d&, format, mip_levels);

		/// Fills in the fields of a \ref frame_buffer object.
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::span<const gpu::image2d_view *const> color_rts, const image2d_view *depth_stencil_rt, cvec2u32 size
		);

		[[nodiscard]] fence create_fence(synchronization_state); // TODO
		/// Calls \p MTL::Device::newSharedEvent().
		[[nodiscard]] timeline_semaphore create_timeline_semaphore(gpu::_details::timeline_semaphore_value_type);

		void reset_fence(fence&); // TODO
		void wait_for_fence(fence&); // TODO

		/// Calls \p MTL::SharedEvent::setSignaledValue().
		void signal_timeline_semaphore(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type);
		/// Calls \p MTL::SharedEvent::signaledValue().
		[[nodiscard]] gpu::_details::timeline_semaphore_value_type query_timeline_semaphore(timeline_semaphore&);
		/// Calls \p MTL::SharedEvent::waitUntilSignaledValue().
		void wait_for_timeline_semaphore(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type);

		[[nodiscard]] timestamp_query_heap create_timestamp_query_heap(std::uint32_t size); // TODO
		void fetch_query_results(timestamp_query_heap&, std::uint32_t first, std::span<std::uint64_t> timestamps); // TODO


		/// Calls \p MTL::Buffer::setLabel().
		void set_debug_name(buffer&, const char8_t*);
		/// Calls \p MTL::Texture::setLabel().
		void set_debug_name(image_base&, const char8_t*);
		/// Calls \p MTL::Texture::setLabel().
		void set_debug_name(image_view_base&, const char8_t*);


		// ray-tracing related
		/// Fills out \p MTL::AccelerationStructureTriangleGeometryDescriptor instances.
		[[nodiscard]] bottom_level_acceleration_structure_geometry
			create_bottom_level_acceleration_structure_geometry(std::span<const raytracing_geometry_view>);

		[[nodiscard]] instance_description get_bottom_level_acceleration_structure_description(
			bottom_level_acceleration_structure&,
			mat44f trans, std::uint32_t id, std::uint8_t mask, std::uint32_t hit_group_offset,
			raytracing_instance_flags
		) const; // TODO

		[[nodiscard]] acceleration_structure_build_sizes get_bottom_level_acceleration_structure_build_sizes(
			const bottom_level_acceleration_structure_geometry&
		); // TODO
		[[nodiscard]] acceleration_structure_build_sizes get_top_level_acceleration_structure_build_sizes(
			std::size_t instance_count
		); // TODO
		[[nodiscard]] bottom_level_acceleration_structure create_bottom_level_acceleration_structure(
			buffer&, std::size_t offset, std::size_t size
		); // TODO
		[[nodiscard]] top_level_acceleration_structure create_top_level_acceleration_structure(
			buffer&, std::size_t offset, std::size_t size
		); // TODO

		void write_descriptor_set_acceleration_structures(
			descriptor_set&, const descriptor_set_layout&, std::size_t first_register,
			std::span<gpu::top_level_acceleration_structure *const>
		); // TODO

		[[nodiscard]] shader_group_handle get_shader_group_handle(const raytracing_pipeline_state&, std::size_t index); // TODO

		[[nodiscard]] raytracing_pipeline_state create_raytracing_pipeline_state(
			std::span<const shader_function> hit_group_shaders,
			std::span<const hit_shader_group> hit_groups,
			std::span<const shader_function> general_shaders,
			std::size_t max_recursion_depth,
			std::size_t max_payload_size,
			std::size_t max_attribute_size,
			const pipeline_resources&
		); // TODO
	private:
		NS::SharedPtr<MTL::Device> _dev; ///< The device.
		NS::SharedPtr<MTL::ResidencySet> _residency_set; ///< Manages all resources.
		context_options _context_opts = context_options::none; ///< Context options.

		/// Initializes all fields of this class.
		device(NS::SharedPtr<MTL::Device> dev, NS::SharedPtr<MTL::ResidencySet> set, context_options opts) :
			_dev(std::move(dev)), _residency_set(std::move(set)), _context_opts(opts) {
		}

		/// Creates a new acceleration structure.
		[[nodiscard]] _details::residency_ptr<MTL::AccelerationStructure> _create_acceleration_structure(
			buffer&, std::size_t offset, std::size_t size
		);
		/// Sets the debug name of the given descriptor set.
		void _maybe_set_descriptor_set_name(MTL::Buffer *buf, const descriptor_set_layout &layout);
	};

	/// Holds a \p MTL::Device.
	class adapter {
		friend context;
	protected:
		/// Initializes this object to empty.
		adapter(std::nullptr_t) {
		}

		/// Creates command queues by calling \p MTL::Device::newCommandQueue().
		[[nodiscard]] std::pair<device, std::vector<command_queue>> create_device(std::span<const queue_family>);
		/// Retrieves device information from the \p MTL::Device.
		[[nodiscard]] adapter_properties get_properties() const;

		/// Checks if this adapter object holds a valid reference to a \p MTL::Device.
		[[nodiscard]] bool is_valid() const {
			return !!_dev;
		}
	private:
		NS::SharedPtr<MTL::Device> _dev; ///< The device.
		context_options _context_opts = context_options::none; ///< Context options.

		/// Initializes all fields of this class.
		adapter(NS::SharedPtr<MTL::Device> dev, context_options opts) : _dev(std::move(dev)), _context_opts(opts) {
		}
	};
}
