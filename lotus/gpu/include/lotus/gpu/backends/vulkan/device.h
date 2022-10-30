#pragma once

/// \file
/// Devices.

#include <optional>

#include "acceleration_structure.h"
#include "commands.h"
#include "descriptors.h"
#include "details.h"
#include "frame_buffer.h"

namespace lotus::gpu::backends::vulkan {
	class adapter;
	class command_list;
	class context;
	class device;


	/// Contains a \p vk::UniqueDevice.
	class device {
		friend adapter;
		friend command_list;
		friend context;
		friend void command_allocator::reset(device&);
	protected:
		/// Creates an empty object.
		device(std::nullptr_t) {
		}

		/// Calls \p vk::UniqueDevice::acquireNextImageKHR().
		[[nodiscard]] back_buffer_info acquire_back_buffer(swap_chain&);
		/// Calls \p vk::UniqueDevice::createSwapchainKHRUnique() to create a new swap chain reusing the old swap
		/// chain.
		void resize_swap_chain_buffers(swap_chain&, cvec2s);

		/// Calls \p vk::UniqueDevice::getQueue();
		[[nodiscard]] command_queue create_command_queue();
		/// Calls \p vk::UniqueDevice::createCommandPoolUnique().
		[[nodiscard]] command_allocator create_command_allocator();
		/// Calls \p vk::UniqueDevice::allocateCommandBuffers() and \p vk::CommandBuffer::begin().
		[[nodiscard]] command_list create_and_start_command_list(command_allocator&);

		/// Calls \p vk::UniqueDevice::createDescriptorPoolUnique().
		[[nodiscard]] descriptor_pool create_descriptor_pool(
			std::span<const descriptor_range> capacity, std::size_t max_num_sets
		);
		/// Calls \p vk::UniqueDevice::allocateDescriptorSetsUnique().
		[[nodiscard]] descriptor_set create_descriptor_set(
			descriptor_pool&, const descriptor_set_layout&
		);
		/// Calls \p vk::UniqueDevice::allocateDescriptorSetsUnique().
		[[nodiscard]] descriptor_set create_descriptor_set(
			descriptor_pool&, const descriptor_set_layout&, std::size_t dynamic_size
		);

		/// Calls \p vk::UniqueDevice::updateDescriptorSets().
		void write_descriptor_set_read_only_images(
			descriptor_set&, const descriptor_set_layout&,
			std::size_t first_register, std::span<const image_view *const>
		);
		/// Calls \p vk::UniqueDevice::updateDescriptorSets().
		void write_descriptor_set_read_write_images(
			descriptor_set&, const descriptor_set_layout&,
			std::size_t first_register, std::span<const image_view *const>
		);
		/// Calls \p vk::UniqueDevice::updateDescriptorSets().
		void write_descriptor_set_read_only_structured_buffers(
			descriptor_set&, const descriptor_set_layout&,
			std::size_t first_register, std::span<const structured_buffer_view>
		);
		/// Calls \p vk::UniqueDevice::updateDescriptorSets().
		void write_descriptor_set_read_write_structured_buffers(
			descriptor_set&, const descriptor_set_layout&,
			std::size_t first_register, std::span<const structured_buffer_view>
		);
		/// Calls \p vk::UniqueDevice::updateDescriptorSets().
		void write_descriptor_set_constant_buffers(
			descriptor_set&, const descriptor_set_layout&,
			std::size_t first_register, std::span<const constant_buffer_view>
		);
		/// Calls \p vk::UniqueDevice::updateDescriptorSets().
		void write_descriptor_set_samplers(
			descriptor_set &set, const descriptor_set_layout &layout,
			std::size_t first_register, std::span<const gpu::sampler *const> samplers
		);

		/// Calls \p vk::UniqueDevice::createShaderModuleUnique().
		[[nodiscard]] shader_binary load_shader(std::span<const std::byte>);

		/// Calls \p vk::UniqueDevice::createSamplerUnique().
		[[nodiscard]] sampler create_sampler(
			filtering minification, filtering magnification, filtering mipmapping,
			float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
			sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
			linear_rgba_f border_color, std::optional<comparison_function> comparison
		);

		/// Calls \p vk::UniqueDevice::createDescriptorSetLayoutUnique().
		[[nodiscard]] descriptor_set_layout create_descriptor_set_layout(
			std::span<const descriptor_range_binding>, shader_stage visible_stages
		);

		/// Calls \p vk::UniqueDevice::createPipelineLayoutUnique().
		[[nodiscard]] pipeline_resources create_pipeline_resources(
			std::span<const gpu::descriptor_set_layout *const>
		);
		/// Calls \p vk::UniqueDevice::createGraphicsPipelineUnique().
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
			std::size_t num_viewports = 1
		);
		/// Calls \p vk::UniqueDevice::createComputePipelineUnique().
		[[nodiscard]] compute_pipeline_state create_compute_pipeline_state(
			const pipeline_resources&, const shader_binary&
		);

		/// Returns the list of cached memory properties.
		[[nodiscard]] std::span<
			const std::pair<memory_type_index, memory_properties>
		> enumerate_memory_types() const {
			return _memory_properties_list;
		}
		/// Calls \p vk::UniqueDevice::allocateMemoryUnique().
		[[nodiscard]] memory_block allocate_memory(std::size_t size, memory_type_index);

		/// Calls \p vk::UniqueDevice::createBuffer() to create the buffer, then calls
		/// \p vk::UniqueDevice::allocateMemory() to allocate memory for it.
		[[nodiscard]] buffer create_committed_buffer(
			std::size_t size, memory_type_index, buffer_usage_mask allowed_usage
		);
		/// Calls \p vk::UniqueDevice::createImage() to create the image, then calls
		/// \p vk::UniqueDevice::allocateMemory() to allocate memory for it.
		[[nodiscard]] image2d create_committed_image2d(
			std::size_t width, std::size_t height, std::size_t array_slices, std::size_t mip_levels,
			format, image_tiling, image_usage_mask allowed_usage
		);
		/// Obtains the layout of the buffer by creating a dummy image object, then calls
		/// \ref create_committed_buffer() to create the buffer.
		[[nodiscard]] std::tuple<buffer, staging_buffer_metadata, std::size_t> create_committed_staging_buffer(
			std::size_t width, std::size_t height, format, memory_type_index, buffer_usage_mask allowed_usage
		);

		/// Creates a temporary \p vk::UniqueImage, then calls \p vk::UniqueDevice::getImageMemoryRequirements2() to
		/// obtain the memory requirements.
		[[nodiscard]] memory::size_alignment get_image_memory_requirements(
			std::size_t width, std::size_t height, std::size_t array_slices, std::size_t mip_levels,
			format, image_tiling, image_usage_mask
		);
		/// Creates a temporary \p vk::UniqueBuffer, then calls \p vk::UniqueDevice::getBufferMemoryRequirements2()
		/// to obtain the memory requirements.
		[[nodiscard]] memory::size_alignment get_buffer_memory_requirements(std::size_t size, buffer_usage_mask);
		/// Calls \p vk::UniqueDevice::createBuffer() to create the buffer, then calls
		/// \p vk::UniqueDevice::bindBufferMemory2() to bind it to the given \ref memory_block.
		[[nodiscard]] buffer create_placed_buffer(
			std::size_t size, buffer_usage_mask allowed_usage, const memory_block &mem, std::size_t offset
		);
		/// Calls \p vk::UniqueDevice::createImage() to create the image, then calls
		/// \p vk::UniqueDevice::bindImageMemory2() to bind it to the given \ref memory_block.
		[[nodiscard]] image2d create_placed_image2d(
			std::size_t width, std::size_t height, std::size_t array_slices, std::size_t mip_levels,
			format, image_tiling, image_usage_mask allowed_usage, const memory_block &mem, std::size_t offset
		);

		/// Calls \ref _map_memory().
		[[nodiscard]] std::byte *map_buffer(buffer&, std::size_t begin, std::size_t length);
		/// Calls \ref _unmap_memory().
		void unmap_buffer(buffer&, std::size_t begin, std::size_t length);
		/// Calls \ref _map_memory().
		[[nodiscard]] std::byte *map_image2d(
			image2d&, subresource_index, std::size_t begin, std::size_t length
		);
		/// Calls \ref _unmap_memory().
		void unmap_image2d(
			image2d&, subresource_index, std::size_t begin, std::size_t length
		);

		/// Calls \p vk::UniqueDevice::createImageViewUnique().
		[[nodiscard]] image2d_view create_image2d_view_from(const image2d&, format, mip_levels);
		/// Fills in the frame buffer structure.
		[[nodiscard]] frame_buffer create_frame_buffer(
			std::span<const gpu::image2d_view *const> color, const image2d_view *depth_stencil, cvec2s size
		);

		/// Calls \p vk::UniqueDevice::createFenceUnique().
		[[nodiscard]] fence create_fence(synchronization_state state);
		/// Calls \p vk::UniqueDevice::createSemaphoreUnique().
		[[nodiscard]] timeline_semaphore create_timeline_semaphore(std::uint64_t);

		/// Calls \p vk::UniqueDevice::resetFences().
		void reset_fence(fence&);
		/// Calls \p vk::UniqueDevice::waitForFences().
		void wait_for_fence(fence&);

		/// Calls \p vk::UniqueDevice::signalSemaphore().
		void signal_timeline_semaphore(timeline_semaphore&, std::uint64_t);
		/// Calls \p vk::UniqueDevice::getSemaphoreCounterValue().
		[[nodiscard]] std::uint64_t query_timeline_semaphore(timeline_semaphore&);
		/// Calls \p vk::UniqueDevice::waitSemaphores().
		void wait_for_timeline_semaphore(timeline_semaphore&, std::uint64_t);

		/// Calls \p vk::UniqueDevice::debugMarkerSetObjectNameEXT().
		void set_debug_name(buffer&, const char8_t*);
		/// Calls \p vk::UniqueDevice::debugMarkerSetObjectNameEXT().
		void set_debug_name(image&, const char8_t*);
		/// Calls \p vk::UniqueDevice::debugMarkerSetObjectNameEXT().
		void set_debug_name(image_view&, const char8_t*);


		// ray-tracing related
		/// Fills in the \p vk::AccelerationStructureBuildGeometryInfoKHR with the given information.
		[[nodiscard]] bottom_level_acceleration_structure_geometry
			create_bottom_level_acceleration_structure_geometry(
				std::span<const std::pair<vertex_buffer_view, index_buffer_view>>
			);
		/// Fills in the \p vk::AccelerationStructureInstanceKHR.
		[[nodiscard]] instance_description get_bottom_level_acceleration_structure_description(
			bottom_level_acceleration_structure&,
			mat44f, std::uint32_t id, std::uint8_t mask, std::uint32_t hit_group_offset // TODO options
		) const;

		/// Returns the result of \p vk::UniqueDevice::getAccelerationStructureBuildSizesKHR().
		[[nodiscard]] acceleration_structure_build_sizes get_bottom_level_acceleration_structure_build_sizes(
			const bottom_level_acceleration_structure_geometry&
		);
		/// Returns the result of \p vk::UniqueDevice::getAccelerationStructureBuildSizesKHR().
		[[nodiscard]] acceleration_structure_build_sizes get_top_level_acceleration_structure_build_sizes(
			const buffer &top_level_buf, std::size_t offset, std::size_t count
		);
		/// Returns the result of \p vk::UniqueDevice::createAccelerationStructureKHRUnique().
		[[nodiscard]] bottom_level_acceleration_structure create_bottom_level_acceleration_structure(
			buffer &buf, std::size_t offset, std::size_t size
		);
		/// Returns the result of \p vk::UniqueDevice::createAccelerationStructureKHRUnique().
		[[nodiscard]] top_level_acceleration_structure create_top_level_acceleration_structure(
			buffer &buf, std::size_t offset, std::size_t size
		);

		/// Calls \p vk::UniqueDevice::updateDescriptorSets().
		void write_descriptor_set_acceleration_structures(
			descriptor_set&, const descriptor_set_layout&, std::size_t first_register,
			std::span<gpu::top_level_acceleration_structure *const> acceleration_structures
		);

		/// Returns the result of \p vk::UniqueDevice::getRayTracingShaderGroupHandlesKHR().
		[[nodiscard]] shader_group_handle get_shader_group_handle(
			const raytracing_pipeline_state &pipeline, std::size_t index
		);

		/// Returns the result of \p vk::UniqueDevice::createRayTracingPipelineKHRUnique().
		[[nodiscard]] raytracing_pipeline_state create_raytracing_pipeline_state(
			std::span<const shader_function> hit_group_shaders, std::span<const hit_shader_group> hit_groups,
			std::span<const shader_function> general_shaders,
			std::size_t max_recursion_depth, std::size_t max_payload_size, std::size_t max_attribute_size,
			const pipeline_resources &rsrc
		);
	private:
		vk::UniqueDevice _device; ///< The device.
		vk::PhysicalDevice _physical_device; ///< The physical device.

		// TODO custom queues
		// queue indices
		std::uint32_t _graphics_compute_queue_family_index = 0; ///< Graphics and compute command queue family index.
		std::uint32_t _compute_queue_family_index = 0; ///< Compute-only command queue family index.

		vk::PhysicalDeviceLimits _device_limits; ///< Device limits.
		vk::PhysicalDeviceMemoryProperties _memory_properties; ///< Memory properties.
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR _raytracing_properties; ///< Raytracing properties.
		/// List of memory properties.
		std::vector<std::pair<memory_type_index, memory_properties>> _memory_properties_list;

		context_options _options = context_options::none; ///< Context options.
		const vk::DispatchLoaderDynamic *_dispatch_loader = nullptr; ///< The dispatch loader.


		/// Finds the best memory type fit for the given requirements and \ref heap_type.
		[[nodiscard]] std::uint32_t _find_memory_type_index(std::uint32_t requirements, memory_properties) const;
		/// Finds the best memory type fit for the given requirements and memory flags.
		[[nodiscard]] std::uint32_t _find_memory_type_index(
			std::uint32_t requirements,
			vk::MemoryPropertyFlags required_on, vk::MemoryPropertyFlags required_off,
			vk::MemoryPropertyFlags optional_on, vk::MemoryPropertyFlags optional_off
		) const;

		/// Maps the given memory, and invalidates the given memory range.
		[[nodiscard]] std::byte *_map_memory(vk::DeviceMemory, std::size_t beg, std::size_t len);
		/// Unmaps the given memory, and flushes the given memory range.
		void _unmap_memory(vk::DeviceMemory, std::size_t beg, std::size_t len);
	};

	/// Contains a \p vk::PhysicalDevice.
	class adapter {
		friend context;
	protected:
		/// Creates an empty object.
		adapter(std::nullptr_t) {
		}

		/// Enumerates all queue families using \p vk::PhysicalDevice::getQueueFamilyProperties(), then creates a
		/// device using \p vk::PhysicalDevice::createDeviceUnique().
		[[nodiscard]] device create_device();
		/// Returns the results of \p vk::PhysicalDevice::getProperties().
		[[nodiscard]] adapter_properties get_properties() const;
	private:
		vk::PhysicalDevice _device; ///< The physical device.
		const vk::DispatchLoaderDynamic *_dispatch_loader; ///< Dispatch loader.
		context_options _options = context_options::none; ///< Context options.

		/// Initializes all fields of the struct.
		adapter(vk::PhysicalDevice dev, context_options opt, const vk::DispatchLoaderDynamic &dispatch) :
			_device(dev), _dispatch_loader(&dispatch), _options(opt) {
		}
	};
}
