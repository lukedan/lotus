#pragma once

/// \file
/// The Metal device.

#include <vector>
#include <span>

#include "lotus/gpu/common.h"
#include "details.h"
#include "acceleration_structure.h"
#include "descriptors.h"
#include "commands.h"
#include "frame_buffer.h"

namespace lotus::gpu::backends::metal {
	class adapter;
	class context;

	/// Holds a \p MTL::Device.
	class device {
		friend adapter;
	public:
		/// Deferred definition to accommodate incomplete metal types.
		device(device&&) noexcept;
		/// Deferred definition to accommodate incomplete metal types.
		device &operator=(device&&) noexcept;
		/// Deferred definition to accommodate incomplete metal types.
		~device();
	protected:
		/// Deferred definition to accommodate incomplete metal types.
		device(std::nullptr_t);

		[[nodiscard]] back_buffer_info acquire_back_buffer(swap_chain&); // TODO
		void resize_swap_chain_buffers(swap_chain&, cvec2u32 size); // TODO

		[[nodiscard]] command_allocator create_command_allocator(queue_family); // TODO
		[[nodiscard]] command_list create_and_start_command_list(command_allocator&); // TODO

		[[nodiscard]] descriptor_pool create_descriptor_pool(std::span<const descriptor_range> capacity, std::size_t max_num_sets); // TODO
		[[nodiscard]] descriptor_set create_descriptor_set(descriptor_pool&, const descriptor_set_layout&); // TODO
		[[nodiscard]] descriptor_set create_descriptor_set(descriptor_pool&, const descriptor_set_layout&, std::size_t dynamic_size); // TODO

		void write_descriptor_set_read_only_images(descriptor_set&, const descriptor_set_layout&, std::size_t first_register, std::span<const image_view_base *const>); // TODO
		void write_descriptor_set_read_write_images(descriptor_set&, const descriptor_set_layout&, std::size_t first_register, std::span<const image_view_base *const>); // TODO

		void write_descriptor_set_read_only_structured_buffers(descriptor_set&, const descriptor_set_layout&, std::size_t first_register, std::span<const structured_buffer_view>); // TODO
		void write_descriptor_set_read_write_structured_buffers(descriptor_set&, const descriptor_set_layout&, std::size_t first_register, std::span<const structured_buffer_view>); // TODO

		void write_descriptor_set_constant_buffers(descriptor_set&, const descriptor_set_layout&, std::size_t first_register, std::span<const constant_buffer_view>); // TODO
		void write_descriptor_set_samplers(descriptor_set&, const descriptor_set_layout&, std::size_t first_register, std::span<const gpu::sampler *const>); // TODO

		[[nodiscard]] shader_binary load_shader(std::span<const std::byte> data); // TODO

		[[nodiscard]] sampler create_sampler(
			filtering minification, filtering magnification, filtering mipmapping,
			float mip_lod_bias, float min_lod, float max_lod, std::optional<float> max_anisotropy,
			sampler_address_mode addressing_u, sampler_address_mode addressing_v, sampler_address_mode addressing_w,
			linear_rgba_f border_color, comparison_function comparison
		); // TODO

		[[nodiscard]] descriptor_set_layout create_descriptor_set_layout(std::span<const descriptor_range_binding>, shader_stage); // TODO

		[[nodiscard]] pipeline_resources create_pipeline_resources(std::span<const gpu::descriptor_set_layout *const>); // TODO
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
		); // TODO
		[[nodiscard]] compute_pipeline_state create_compute_pipeline_state(
			const pipeline_resources&, const shader_binary&
		); // TODO

		[[nodiscard]] std::span<const std::pair<memory_type_index, memory_properties>> enumerate_memory_types() const; // TODO
		[[nodiscard]] memory_block allocate_memory(std::size_t size, memory_type_index); // TODO
		[[nodiscard]] buffer create_committed_buffer(std::size_t size, memory_type_index, buffer_usage_mask); // TODO
		[[nodiscard]] image2d create_committed_image2d(cvec2u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask); // TODO
		[[nodiscard]] image3d create_committed_image3d(cvec3u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask); // TODO
		[[nodiscard]] std::tuple<buffer, staging_buffer_metadata, std::size_t> create_committed_staging_buffer(cvec2u32 size, format, memory_type_index, buffer_usage_mask); // TODO

		[[nodiscard]] memory::size_alignment get_image2d_memory_requirements(cvec2u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask); // TODO
		[[nodiscard]] memory::size_alignment get_image3d_memory_requirements(cvec3u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask); // TODO
		[[nodiscard]] memory::size_alignment get_buffer_memory_requirements(std::size_t size, buffer_usage_mask usages); // TODO
		[[nodiscard]] buffer create_placed_buffer(std::size_t size, buffer_usage_mask, const memory_block&, std::size_t offset); // TODO
		[[nodiscard]] image2d create_placed_image2d(cvec2u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask, const memory_block&, std::size_t offset); // TODO
		[[nodiscard]] image3d create_placed_image3d(cvec3u32 size, std::uint32_t mip_levels, format, image_tiling, image_usage_mask, const memory_block&, std::size_t offset); // TODO

		[[nodiscard]] std::byte *map_buffer(buffer&); // TODO
		void unmap_buffer(buffer&); // TODO
		void flush_mapped_buffer_to_host(buffer&, std::size_t begin, std::size_t length); // TODO
		void flush_mapped_buffer_to_device(buffer&, std::size_t begin, std::size_t length); // TODO

		[[nodiscard]] image2d_view create_image2d_view_from(const image2d&, format, mip_levels); // TODO
		[[nodiscard]] image3d_view create_image3d_view_from(const image3d&, format, mip_levels); // TODO

		[[nodiscard]] frame_buffer create_frame_buffer(std::span<const image2d_view *const>, const image2d_view*, cvec2u32); // TODO

		[[nodiscard]] fence create_fence(synchronization_state); // TODO
		[[nodiscard]] timeline_semaphore create_timeline_semaphore(gpu::_details::timeline_semaphore_value_type); // TODO

		void reset_fence(fence&); // TODO
		void wait_for_fence(fence&); // TODO

		void signal_timeline_semaphore(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type); // TODO
		[[nodiscard]] gpu::_details::timeline_semaphore_value_type query_timeline_semaphore(timeline_semaphore&); // TODO
		void wait_for_timeline_semaphore(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type); // TODO

		[[nodiscard]] timestamp_query_heap create_timestamp_query_heap(std::uint32_t size); // TODO
		void fetch_query_results(timestamp_query_heap&, std::uint32_t first, std::span<std::uint64_t> timestamps); // TODO


		void set_debug_name(buffer&, const char8_t*); // TODO
		void set_debug_name(image_base&, const char8_t*); // TODO
		void set_debug_name(image_view_base&, const char8_t*); // TODO


		// ray-tracing related
		[[nodiscard]] bottom_level_acceleration_structure_geometry
			create_bottom_level_acceleration_structure_geometry(std::span<const raytracing_geometry_view>); // TODO

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
			std::size_t max_recursion_depth, std::size_t max_payload_size, std::size_t max_attribute_size,
			const pipeline_resources&
		); // TODO
	private:
		_details::metal_ptr<MTL::Device> _dev; ///< The device.

		/// Initializes \ref _dev.
		explicit device(_details::metal_ptr<MTL::Device>);
	};

	/// Holds a \p MTL::Device.
	class adapter {
		friend context;
	public:
		/// Deferred definition to accommodate incomplete metal types.
		adapter(adapter&&) noexcept;
		/// Deferred definition to accommodate incomplete metal types.
		adapter(const adapter&);
		/// Deferred definition to accommodate incomplete metal types.
		adapter &operator=(adapter&&) noexcept;
		/// Deferred definition to accommodate incomplete metal types.
		adapter &operator=(const adapter&);
		/// Deferred definition to accommodate incomplete metal types.
		~adapter();
	protected:
		/// Deferred definition to accommodate incomplete metal types.
		adapter(std::nullptr_t);

		/// Creates command queues by calling \p MTL::Device::newCommandQueue().
		[[nodiscard]] std::pair<device, std::vector<command_queue>> create_device(std::span<const queue_family>);
		/// Retrieves device information from the \p MTL::Device.
		[[nodiscard]] adapter_properties get_properties() const;

		/// Checks if this adapter object holds a valid reference to a \p MTL::Device.
		[[nodiscard]] bool is_valid() const {
			return _dev.is_valid();
		}
	private:
		_details::metal_ptr<MTL::Device> _dev; ///< The device.

		/// Initializes \p _dev.
		adapter(_details::metal_ptr<MTL::Device>);
	};
}
