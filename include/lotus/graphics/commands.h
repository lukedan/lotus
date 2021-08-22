#pragma once

/// \file
/// Command related classes.

#include <span>

#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "pass.h"
#include "resources.h"
#include "frame_buffer.h"
#include "synchronization.h"

namespace lotus::graphics {
	class device;
	class command_allocator;

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

		/// Resets this command list. This is only valid when the command allocator allows resetting individual
		/// command lists.
		void reset(command_allocator&);

		/// Starts recording to the command buffer. \ref reset() must be called if this command buffer has been used
		/// previously.
		void start();
		/// Starts a rendering pass.
		void begin_pass(
			const pass_resources &p, const frame_buffer &fb,
			std::span<linear_rgba_f> clear_colors, float clear_depth, std::uint8_t clear_stencil
		) {
			backend::command_list::begin_pass(p, fb, clear_colors, clear_depth, clear_stencil);
		}

		/// Binds vertex buffers for rendering.
		void bind_vertex_buffers(std::size_t start, std::span<vertex_buffer> buffers) {
			backend::command_list::bind_vertex_buffers(start, buffers);
		}

		/// Inserts a copy operation between the two buffers.
		void copy_buffer(buffer &from, std::size_t off1, buffer &to, std::size_t off2, std::size_t size) {
			backend::command_list::copy_buffer(from, off1, to, off2, size);
		}

		/// Inserts an resource barrier. This should only be called out of render passes.
		void resource_barrier(std::span<image_barrier> images, std::span<buffer_barrier> buffers) {
			backend::command_list::resource_barrier(images, buffers);
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

	/// Used for allocating commands.
	class command_allocator : public backend::command_allocator {
		friend device;
	public:
		/// No copy construction.
		command_allocator(const command_allocator&) = delete;
		/// No copy assignment.
		command_allocator &operator=(const command_allocator&) = delete;

		/// Resets this command allocator and all \ref command_list allocated from it.
		void reset(device&);
	protected:
		/// Implicit conversion from base type.
		command_allocator(backend::command_allocator base) : backend::command_allocator(std::move(base)) {
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
		void submit_command_lists(std::span<const command_list*> lists, fence *on_completion) {
			backend::command_queue::submit_command_lists(lists, on_completion);
		}
		/// Presents the current back buffer in the swap chain. The fence is used to determine when the back buffer
		/// has finished presenting and the next frame using the same back buffer can start.
		void present(swap_chain &target, fence *on_completion) {
			backend::command_queue::present(target, on_completion);
		}
	protected:
		/// Initializes the backend command queue.
		command_queue(backend::command_queue q) : backend::command_queue(std::move(q)) {
		}
	};


	inline void command_list::reset(command_allocator &alloc) {
		backend::command_list::reset(alloc);
	}

	inline void command_list::start() {
		backend::command_list::start();
	}
}
