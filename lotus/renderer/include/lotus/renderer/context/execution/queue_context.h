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
		/// Default move construction.
		queue_context(queue_context&&) = default;
		/// No copy construction.
		queue_context(const queue_context&) = delete;
		/// No move or copy assignment.
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
		/// Additional operations (e.g. synchronization) and data required for each command. The order of operations
		/// are:
		/// 1. Acquire dependencies.
		/// 2. Perform pre-barrier transitions.
		/// 3. Insert pre-timestamp.
		/// 4. Execute the command.
		/// 5. Insert post-timestamp.
		/// 6. Release dependencies.
		struct _command_operation {
			/// Dependencies that need to be acquired before this command from all other queues. 0 means no
			/// dependency.
			short_vector<gpu::timeline_semaphore::value_type, 4> acquire_dependencies;

			/// Image transitions to execute before the command.
			std::vector<gpu::image_barrier> pre_image_transitions;
			/// Buffer transitions to execute after the command.
			std::vector<gpu::buffer_barrier> pre_buffer_transitions;

			bool insert_pre_timestamp = false; ///< Whether to insert a timestamp before this command.

			// command execution

			bool insert_post_timestamp = false; ///< Whether to insert a timestamp after this command.

			/// Image transitions to execute after this command.
			std::vector<gpu::image_barrier> post_image_transitions;
			/// Buffer transitions to execute after this command.
			std::vector<gpu::buffer_barrier> post_buffer_transitions;

			/// The value to set the timeline semaphore to after this command.
			std::optional<gpu::timeline_semaphore::value_type> release_dependency;
		};

		batch_context &_batch_ctx; ///< The associated batch context.
		_details::queue_data &_q; ///< The associated command queue.

		// events collected during pseudo execution
		std::vector<_command_operation> _cmd_ops; ///< Operations to execute for all commands.

		// execution state
		gpu::command_allocator *_cmd_alloc = nullptr; ///< Command allocator used by this queue.
		gpu::command_list *_list = nullptr; ///< Current command list.
		gpu::timestamp_query_heap *_timestamps = nullptr; ///< Timestamp query heap for this batch on this queue.
		std::uint32_t _timestamp_count = 0; ///< Total timestamp count in this batch.

		queue_submission_index _command_index = zero; ///< Index of the next command.
		std::uint32_t _timestamp_index = 0; ///< Index of the next timestamp to be inserted.

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
		/// Does nothing.
		void _execute(const commands::start_of_batch&) {
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
		/// Does nothing - dependency tracking is handled explicitly during pseudo execution.
		void _execute(const commands::release_dependency&) {
		}
		/// Does nothing - dependency tracking is handled explicitly during pseudo execution.
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
