#pragma once

/// \file
/// Metal command buffers.

#include "lotus/color.h"
#include "lotus/gpu/common.h"
#include "details.h"
#include "frame_buffer.h"
#include "pipeline.h"
#include "resources.h"

namespace lotus::gpu::backends::metal {
	class adapter;
	class device;

	// TODO
	class command_allocator {
	protected:
		command_allocator(std::nullptr_t); // TODO

		void reset(device&); // TODO
	private:
	};

	// TODO
	class command_list {
	protected:
		command_list(std::nullptr_t); // TODO

		void reset_and_start(command_allocator&); // TODO

		void begin_pass(const frame_buffer&, const frame_buffer_access&); // TODO

		void bind_pipeline_state(const graphics_pipeline_state&); // TODO
		void bind_pipeline_state(const compute_pipeline_state&); // TODO
		void bind_vertex_buffers(std::size_t start, std::span<const vertex_buffer>); // TODO
		void bind_index_buffer(const buffer&, std::size_t offset, index_format); // TODO
		void bind_graphics_descriptor_sets(const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>); // TODO
		void bind_compute_descriptor_sets(const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>); // TODO

		void set_viewports(std::span<const viewport>); // TODO
		void set_scissor_rectangles(std::span<const aab2i>); // TODO

		void copy_buffer(const buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size); // TODO
		void copy_image2d(image2d &from, subresource_index sub1, aab2u32 region, image2d &to, subresource_index sub2, cvec2u32 off); // TODO
		void copy_buffer_to_image(const buffer &from, std::size_t byte_offset, staging_buffer_metadata, image2d &to, subresource_index subresource, cvec2u32 off); // TODO

		void draw_instanced(std::size_t first_vertex, std::size_t vertex_count, std::size_t first_instance, std::size_t instance_count); // TODO
		void draw_indexed_instanced(std::size_t first_index, std::size_t index_count, std::size_t first_vertex, std::size_t first_instance, std::size_t instance_count); // TODO
		void run_compute_shader(std::uint32_t x, std::uint32_t y, std::uint32_t z); // TODO

		void resource_barrier(std::span<const image_barrier>, std::span<const buffer_barrier>); // TODO

		void end_pass(); // TODO

		void query_timestamp(timestamp_query_heap&, std::uint32_t index); // TODO
		void resolve_queries(timestamp_query_heap&, std::uint32_t first, std::uint32_t count); // TODO

		void insert_marker(const char8_t*, linear_rgba_u8 color); // TODO
		void begin_marker_scope(const char8_t*, linear_rgba_u8 color); // TODO
		void end_marker_scope(); // TODO

		void finish(); // TODO


		// ray-tracing related
		void build_acceleration_structure(const bottom_level_acceleration_structure_geometry&, bottom_level_acceleration_structure &output, buffer &scratch, std::size_t scratch_offset); // TODO
		void build_acceleration_structure(const buffer &instances, std::size_t offset, std::size_t count, top_level_acceleration_structure &output, buffer &scratch, std::size_t scratch_offset); // TODO

		void bind_pipeline_state(const raytracing_pipeline_state&); // TODO
		void bind_ray_tracing_descriptor_sets(const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>); // TODO
		void trace_rays(constant_buffer_view ray_generation, shader_record_view miss_shaders, shader_record_view hit_groups, std::size_t width, std::size_t height, std::size_t depth); // TODO


		[[nodiscard]] bool is_valid() const; // TODO
	};

	/// Holds a \p MTL::CommandQueue.
	class command_queue {
		friend adapter;
	public:
		/// Deferred definition to accommodate incomplete metal types.
		command_queue(command_queue&&) noexcept;
		/// Deferred definition to accommodate incomplete metal types.
		command_queue(const command_queue&);
		/// Deferred definition to accommodate incomplete metal types.
		command_queue &operator=(command_queue&&) noexcept;
		/// Deferred definition to accommodate incomplete metal types.
		command_queue &operator=(const command_queue&);
		/// Deferred definition to accommodate incomplete metal types.
		~command_queue();
	protected:
		command_queue(std::nullptr_t); // TODO

		[[nodiscard]] double get_timestamp_frequency(); // TODO

		void submit_command_lists(std::span<const gpu::command_list *const>, queue_synchronization); // TODO
		[[nodiscard]] swap_chain_status present(swap_chain&); // TODO

		void signal(fence&); // TODO
		void signal(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type); // TODO

		[[nodiscard]] queue_capabilities get_capabilities() const; // TODO

		[[nodiscard]] bool is_valid() const; // TODO
	private:
		_details::metal_ptr<MTL::CommandQueue> _q; ///< The command queue.

		/// Initializes \ref _q.
		command_queue(_details::metal_ptr<MTL::CommandQueue>);
	};
}
