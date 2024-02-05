#pragma once

/// \file
/// Classes that are used during a \ref lotus::renderer::context's execution.

#include <cstddef>
#include <deque>
#include <vector>
#include <unordered_map>
#include <memory>

#include "lotus/utils/static_function.h"
#include "lotus/gpu/resources.h"
#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/pipeline.h"
#include "lotus/gpu/synchronization.h"
#include "lotus/gpu/commands.h"
#include "lotus/renderer/common.h"
#include "lotus/renderer/context/misc.h"
#include "lotus/renderer/context/resources.h"
#include "lotus/renderer/context/commands.h"
#include "common.h"
#include "descriptors.h"

namespace lotus::renderer::commands {
	struct start_timer;
	struct end_timer;
}

namespace lotus::renderer::execution {
	/// Manages the execution of commands in a batch on one command queue.
	class queue_context {
		friend queue_pseudo_context;
		friend batch_context;
	public:
		/// Initializes this context.
		queue_context(batch_context&, _details::queue_data&);
		/// Default move constructor.
		queue_context(queue_context&&) = default;
		/// No move construction.
		queue_context(const queue_context&) = delete;
		/// Default move assignment.
		queue_context &operator=(queue_context&&) = default;
		/// No move assignment.
		queue_context &operator=(const queue_context&) = delete;


		/// Starts the execution phase.
		void start_execution();
		/// Executes the next command.
		void execute_next_command();
		/// Returns whether execution has finished.
		[[nodiscard]] bool is_finished() const;
		/// Finishes the execution phase.
		void finish_execution();


		batch_statistics_early early_statistics; ///< Accumulated statistics.
	private:
		batch_context &_batch_ctx; ///< The associated batch context.
		_details::queue_data &_q; ///< The associated command queue.

		// events collected during pseudo execution
		/// Indices of the commands to insert timers before.
		std::vector<queue_submission_index> _timestamp_command_indices;
		/// List of all release dependency events, sorted based on the \ref queue_submission_index. Note that unlike
		/// \ref queue_pseudo_context, these release events happen *after* the corresponding commands.
		std::vector<release_dependency_event<gpu::timeline_semaphore::value_type>> _release_dependency_events;
		/// List of all acquire dependency events that needs to happen *before* a command, sorted based on
		/// \ref _acquire_dependency_event::acquire_command_index.
		std::vector<acquire_dependency_event<gpu::timeline_semaphore::value_type>> _acquire_dependency_events;
		/// Image barriers for each command.
		std::vector<std::vector<gpu::image_barrier>> _image_transitions;
		/// Buffer barriers for each command.
		std::vector<std::vector<gpu::buffer_barrier>> _buffer_transitions;

		// execution state
		gpu::command_allocator *_cmd_alloc = nullptr; ///< Command allocator used by this queue.
		gpu::command_list *_list = nullptr; ///< Current command list.
		gpu::timestamp_query_heap *_timestamps = nullptr; ///< Timestamp query heap for this batch on this queue.

		/// All waits that have been issued before any other commands have been executed or any other signal events
		/// have been issued. Since waits are issued on command list submission, this is used to reduce the number of
		/// command lists that we submit.
		std::vector<gpu::timeline_semaphore_synchronization> _pending_waits;

		queue_submission_index _command_index = zero; ///< Index of the next command.

		decltype(_timestamp_command_indices)::const_iterator _next_timestamp; ///< Next timestamp.
		decltype(_release_dependency_events)::const_iterator _next_release_event; ///< Next release dependency event.
		decltype(_acquire_dependency_events)::const_iterator _next_acquire_event; ///< Next acquire dependency event.

		// pass-related execution state
		bool _within_pass = false; ///< Whether we're inside a render pass.
		std::vector<gpu::format> _color_rt_formats; ///< Formats of color render targets that are being rendered to.
		/// Format of the depth-stencil render target that is being rendered to.
		gpu::format _depth_stencil_rt_format = gpu::format::none;


		/// Creates the command list if necessary, and returns the current command list.
		[[nodiscard]] gpu::command_list &_get_command_list();
		/// Submits the current command list for execution. The command list will notify the provided \ref gpu::fence
		/// and \ref gpu::timeline_semaphore_synchronization objects, and will wait for all semaphores in
		/// \ref _pending_waits. \ref _pending_waits will be cleared after this call. If no commands have been
		/// recorded since last submission, an empty submission will be created with aforementioned synchronization
		/// operations.
		void _flush_command_list(
			gpu::fence*, std::span<const gpu::timeline_semaphore_synchronization> notify_events
		);
		/// Binds descriptor sets.
		void _bind_descriptor_sets(const pipeline_resources_info&, descriptor_set_bind_point);

		/// Calls \p std::unreachable().
		void _execute(const commands::invalid&) {
			std::unreachable();
		}
		/// Executes a buffer copy command.
		void _execute(const commands::copy_buffer&);
		/// Executes a buffer-to-image copy command.
		void _execute(const commands::copy_buffer_to_image&);
		/// Builds the bottom-level acceleration structure.
		void _execute(const commands::build_blas&);
		/// Builds the top-level acceleration structure.
		void _execute(const commands::build_tlas&);
		/// Starts a render pass.
		void _execute(const commands::begin_pass&);
		/// Draws a mesh.
		void _execute(const commands::draw_instanced&);
		/// Ends the render pass.
		void _execute(const commands::end_pass&);
		/// Executes a compute dispatch command.
		void _execute(const commands::dispatch_compute&);
		/// Executes a raytrace command.
		void _execute(const commands::trace_rays&);
		/// Executes a present command.
		void _execute(const commands::present&);
		/// Does nothing - dependencies are tracked during pseudo-execution and executed manually.
		void _execute(const commands::release_dependency&) {
		}
		/// Does nothing - dependencies are tracked during pseudo-execution and executed manually.
		void _execute(const commands::acquire_dependency&) {
		}
		/// Does nothing - timers are tracked during pseudo-execution and executed manually.
		void _execute(const commands::start_timer&) {
		}
		/// Does nothing - timers are tracked during pseudo-execution and executed manually.
		void _execute(const commands::end_timer&) {
		}
		/// Pauses for the debugger.
		void _execute(const commands::pause_for_debugging&) {
			pause_for_debugger();
		}

		/// Returns the associated device.
		[[nodiscard]] gpu::device &_get_device() const;
		/// Returns the resolve data associated with this queue.
		[[nodiscard]] batch_resolve_data::queue_data &_get_queue_resolve_data();
	};
}
