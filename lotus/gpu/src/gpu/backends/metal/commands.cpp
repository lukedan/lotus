#include "lotus/gpu/backends/metal/commands.h"

/// \file
/// Implementation of Metal command buffers.

#include <Metal/Metal.hpp>

namespace lotus::gpu::backends::metal {
	void command_allocator::reset(device&) {
	}


	void command_list::reset_and_start(command_allocator &alloc) {
		_buf = _details::take_ownership(alloc._q->commandBuffer());
	}

	void command_list::begin_pass(const frame_buffer&, const frame_buffer_access&) {
		// TODO
	}

	void command_list::bind_pipeline_state(const graphics_pipeline_state&) {
		// TODO
	}

	void command_list::bind_pipeline_state(const compute_pipeline_state&) {
		// TODO
	}

	void command_list::bind_vertex_buffers(std::size_t start, std::span<const vertex_buffer>) {
		// TODO
	}

	void command_list::bind_index_buffer(const buffer&, std::size_t offset, index_format) {
		// TODO
	}

	void command_list::bind_graphics_descriptor_sets(const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>) {
		// TODO
	}

	void command_list::bind_compute_descriptor_sets(const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>) {
		// TODO
	}

	void command_list::set_viewports(std::span<const viewport>) {
		// TODO
	}

	void command_list::set_scissor_rectangles(std::span<const aab2i>) {
		// TODO
	}

	void command_list::copy_buffer(const buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size) {
		// TODO
	}

	void command_list::copy_image2d(image2d &from, subresource_index sub1, aab2u32 region, image2d &to, subresource_index sub2, cvec2u32 off) {
		// TODO
	}

	void command_list::copy_buffer_to_image(const buffer &from, std::size_t byte_offset, staging_buffer_metadata, image2d &to, subresource_index subresource, cvec2u32 off) {
		// TODO
	}

	void command_list::draw_instanced(std::size_t first_vertex, std::size_t vertex_count, std::size_t first_instance, std::size_t instance_count) {
		// TODO
	}

	void command_list::draw_indexed_instanced(std::size_t first_index, std::size_t index_count, std::size_t first_vertex, std::size_t first_instance, std::size_t instance_count) {
		// TODO
	}

	void command_list::run_compute_shader(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
		// TODO
	}

	void command_list::resource_barrier(std::span<const image_barrier>, std::span<const buffer_barrier>) {
		// TODO
	}

	void command_list::end_pass() {
		// TODO
	}

	void command_list::query_timestamp(timestamp_query_heap&, std::uint32_t index) {
		// TODO
	}

	void command_list::resolve_queries(timestamp_query_heap&, std::uint32_t first, std::uint32_t count) {
		// TODO
	}

	void command_list::insert_marker(const char8_t*, linear_rgba_u8) {
		// TODO
	}

	void command_list::begin_marker_scope(const char8_t*, linear_rgba_u8) {
		// TODO
	}

	void command_list::end_marker_scope() {
		// TODO
	}

	void command_list::finish() {
		// TODO
	}

	void command_list::build_acceleration_structure(
		const bottom_level_acceleration_structure_geometry&,
		bottom_level_acceleration_structure &output,
		buffer &scratch,
		std::size_t scratch_offset
	) {
		// TODO
	}

	void command_list::build_acceleration_structure(
		const buffer &instances,
		std::size_t offset,
		std::size_t count,
		top_level_acceleration_structure &output,
		buffer &scratch,
		std::size_t scratch_offset
	) {
		// TODO
	}

	void command_list::bind_pipeline_state(const raytracing_pipeline_state&) {
		// TODO
	}

	void command_list::bind_ray_tracing_descriptor_sets(const pipeline_resources&, std::size_t first, std::span<const gpu::descriptor_set *const>) {
		// TODO
	}

	void command_list::trace_rays(
		constant_buffer_view ray_generation,
		shader_record_view miss_shaders,
		shader_record_view hit_groups,
		std::size_t width,
		std::size_t height,
		std::size_t depth
	) {
		// TODO
	}


	double command_queue::get_timestamp_frequency() {
		// TODO
	}

	void command_queue::submit_command_lists(std::span<const gpu::command_list *const>, queue_synchronization) {
		// TODO
	}

	swap_chain_status command_queue::present(swap_chain&) {
		// TODO
	}

	void command_queue::signal(fence&) {
		// TODO
	}

	void command_queue::signal(timeline_semaphore&, gpu::_details::timeline_semaphore_value_type) {
		// TODO
	}

	queue_capabilities command_queue::get_capabilities() const {
		// TODO
	}
}
