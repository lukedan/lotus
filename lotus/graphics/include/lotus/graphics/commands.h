#pragma once

/// \file
/// Command related classes.

#include <span>
#include <initializer_list>

#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "lotus/math/aab.h"
#include "lotus/color.h"
#include "pass.h"
#include "resources.h"
#include "frame_buffer.h"
#include "pipeline.h"
#include "synchronization.h"

namespace lotus::graphics {
	class device;
	class command_allocator;


	/// Used for allocating commands.
	class command_allocator : public backend::command_allocator {
		friend device;
	public:
		/// No copy construction.
		command_allocator(const command_allocator&) = delete;
		/// No copy assignment.
		command_allocator &operator=(const command_allocator&) = delete;

		/// Resets this command allocator and all \ref command_list allocated from it. This should not be called if
		/// any command list allocated from this object is still being executed.
		void reset(device&);
	protected:
		/// Implicit conversion from base type.
		command_allocator(backend::command_allocator base) : backend::command_allocator(std::move(base)) {
		}
	};

	/// A list of commands submitted through a queue.
	class command_list : public backend::command_list {
		friend device;
	public:
		/// Creates an empty command list.
		command_list(std::nullptr_t) : backend::command_list(nullptr) {
		}
		/// Move constructor.
		command_list(command_list &&src) : backend::command_list(std::move(src)) {
		}
		/// No copy construction.
		command_list(const command_list&) = delete;
		/// Move assignment.
		command_list &operator=(command_list &&src) {
			backend::command_list::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		command_list &operator=(const command_list&) = delete;

		/// Resets this command list and starts recording commands to it. This is only valid when the command
		/// allocator allows resetting individual command lists, and should only be called if this command list has
		/// finished executing.
		void reset_and_start(command_allocator &alloc) {
			backend::command_list::reset_and_start(alloc);
		}

		/// Starts a rendering pass.
		void begin_pass(
			const pass_resources &p, const frame_buffer &fb,
			std::span<const linear_rgba_f> clear_colors, float clear_depth, std::uint8_t clear_stencil
		) {
			backend::command_list::begin_pass(p, fb, clear_colors, clear_depth, clear_stencil);
		}
		/// \overload
		void begin_pass(
			const pass_resources &p, const frame_buffer &fb,
			std::initializer_list<linear_rgba_f> clear_colors, float clear_depth, std::uint8_t clear_stencil
		) {
			begin_pass(p, fb, { clear_colors.begin(), clear_colors.end() }, clear_depth, clear_stencil);
		}

		/// Sets all state of the fixed-function graphics pipeline.
		void bind_pipeline_state(const graphics_pipeline_state &state) {
			backend::command_list::bind_pipeline_state(state);
		}
		/// Sets all state of the compute pipeline.
		void bind_pipeline_state(const compute_pipeline_state &state) {
			backend::command_list::bind_pipeline_state(state);
		}
		/// Binds vertex buffers for rendering.
		void bind_vertex_buffers(std::size_t start, std::span<const vertex_buffer> buffers) {
			backend::command_list::bind_vertex_buffers(start, buffers);
		}
		/// \overload
		void bind_vertex_buffers(std::size_t start, std::initializer_list<vertex_buffer> buffers) {
			bind_vertex_buffers(start, { buffers.begin(), buffers.end() });
		}
		/// Binds an index buffer for rendering.
		void bind_index_buffer(const buffer &buf, std::size_t offset, index_format fmt) {
			backend::command_list::bind_index_buffer(buf, offset, fmt);
		}
		/// Binds descriptor sets for rendering.
		void bind_graphics_descriptor_sets(
			const pipeline_resources &rsrc, std::size_t first, std::span<const descriptor_set *const> sets
		) {
			backend::command_list::bind_graphics_descriptor_sets(rsrc, first, sets);
		}
		/// \overload
		void bind_graphics_descriptor_sets(
			const pipeline_resources &rsrc, std::size_t first, std::initializer_list<const descriptor_set*> sets
		) {
			bind_graphics_descriptor_sets(rsrc, first, { sets.begin(), sets.end() });
		}
		/// Binds descriptor sets for compute.
		void bind_compute_descriptor_sets(
			const pipeline_resources &rsrc, std::size_t first, std::span<const descriptor_set *const> sets
		) {
			backend::command_list::bind_compute_descriptor_sets(rsrc, first, sets);
		}
		/// \overload
		void bind_compute_descriptor_sets(
			const pipeline_resources &rsrc, std::size_t first, std::initializer_list<const descriptor_set*> sets
		) {
			bind_compute_descriptor_sets(rsrc, first, { sets.begin(), sets.end() });
		}

		/// Sets the viewports used for rendering.
		void set_viewports(std::span<const viewport> vps) {
			backend::command_list::set_viewports(vps);
		}
		/// \overload
		void set_viewports(std::initializer_list<viewport> vps) {
			set_viewports({ vps.begin(), vps.end() });
		}
		/// Sets the list of scissor rectangles.
		void set_scissor_rectangles(std::span<const aab2i> scissor) {
			backend::command_list::set_scissor_rectangles(scissor);
		}
		/// \overload
		void set_scissor_rectangles(std::initializer_list<aab2i> scissor) {
			set_scissor_rectangles({ scissor.begin(), scissor.end() });
		}

		/// Inserts a copy operation between the two buffers.
		void copy_buffer(buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size) {
			backend::command_list::copy_buffer(from, off1, to, off2, size);
		}
		/// Inserts a copy operation between the two subresources.
		void copy_image2d(
			image2d &from, subresource_index sub1, aab2s region, image2d &to, subresource_index sub2, cvec2s off
		) {
			backend::command_list::copy_image2d(from, sub1, region, to, sub2, off);
		}
		/// Inserts a copy operation from a buffer to an image.
		void copy_buffer_to_image(
			buffer &from, std::size_t byte_offset, std::size_t row_pitch, aab2s region,
			image2d &to, subresource_index subresource, cvec2s off
		) {
			backend::command_list::copy_buffer_to_image(from, byte_offset, row_pitch, region, to, subresource, off);
		}

		/// Instanced draw operation.
		void draw_instanced(
			std::size_t first_vertex, std::size_t vertex_count,
			std::size_t first_instance, std::size_t instance_count
		) {
			backend::command_list::draw_instanced(first_vertex, vertex_count, first_instance, instance_count);
		}
		/// Indexed instanced draw operation.
		void draw_indexed_instanced(
			std::size_t first_index, std::size_t index_count,
			std::size_t first_vertex,
			std::size_t first_instance, std::size_t instance_count
		) {
			backend::command_list::draw_indexed_instanced(
				first_index, index_count, first_vertex, first_instance, instance_count
			);
		}
		/// Runs the currently bound compute shader.
		void run_compute_shader(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
			backend::command_list::run_compute_shader(x, y, z);
		}

		/// Inserts an resource barrier. This should only be called out of render passes.
		void resource_barrier(std::span<const image_barrier> images, std::span<const buffer_barrier> buffers) {
			backend::command_list::resource_barrier(images, buffers);
		}
		/// \overload
		void resource_barrier(
			std::initializer_list<image_barrier> images, std::initializer_list<buffer_barrier> buffers
		) {
			resource_barrier({ images.begin(), images.end() }, { buffers.begin(), buffers.end() });
		}

		/// Ends a rendering pass.
		void end_pass() {
			backend::command_list::end_pass();
		}
		/// Finishes recording to this command list.
		void finish() {
			backend::command_list::finish();
		}
	protected:
		/// Implicit conversion from base type.
		command_list(backend::command_list base) : backend::command_list(std::move(base)) {
		}
	};

	/// A command queue.
	class command_queue : public backend::command_queue {
		friend device;
	public:
		/// No copy construction.
		command_queue(const command_queue&) = delete;
		/// No copy assignment.
		command_queue &operator=(const command_queue&) = delete;

		/// Submits all given command lists for execution. These command lists are guaranteed to execute after all
		/// command lists in the last call to this function has finished, but multiple command lists in a single call
		/// may start simultaneously or overlap.
		void submit_command_lists(std::span<const command_list *const> lists, fence *on_completion) {
			backend::command_queue::submit_command_lists(lists, on_completion);
		}
		/// \overload
		void submit_command_lists(std::initializer_list<command_list*> lists, fence *on_completion) {
			submit_command_lists({ lists.begin(), lists.end() }, on_completion);
		}
		/// Presents the current back buffer in the swap chain.
		void present(swap_chain &target) {
			backend::command_queue::present(target);
		}
		/// Signals the given fence once the GPU has finished all previous command lists.
		void signal(fence &f) {
			backend::command_queue::signal(f);
		}
	protected:
		/// Initializes the backend command queue.
		command_queue(backend::command_queue q) : backend::command_queue(std::move(q)) {
		}
	};
}
