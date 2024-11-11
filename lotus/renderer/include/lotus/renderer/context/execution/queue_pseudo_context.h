#pragma once

/// \file
/// Queue context used specifically for pseudo-execution.

#include "lotus/renderer/context/commands.h"
#include "common.h"

namespace lotus::renderer::execution {
	/// Manages the pseudo execution phase of commands in a batch on one command queue.
	class queue_pseudo_context {
		friend batch_context;
	public:
		/// Initializes this pseudo-execution context.
		queue_pseudo_context(batch_context&, queue_context&, _details::queue_data&);

		/// Returns the next command for pseudo-execution.
		[[nodiscard]] const command &get_next_pseudo_execution_command() const;
		/// Pseudo-executes the next command in this queue.
		void pseudo_execute_next_command();
		/// Returns whether pseudo-execution has been blocked *for this queue* due to a dependency.
		[[nodiscard]] bool is_pseudo_execution_blocked() const;
		/// Returns whether pseudo-execution has finished *for this queue*.
		[[nodiscard]] bool is_pseudo_execution_finished() const;

		/// Gathers all dependency acquisition events, getting rid of all unnecessary ones, and marks commands on
		/// other queues that releases dependencies.
		void process_dependency_acquisitions();
		/// Assigns concrete semaphore values to all commands that releases dependencies.
		void gather_semaphore_values();
		/// Updates all dependency events in the \ref batch_context with concrete semaphore values.
		void finalize_dependency_processing();
	private:
		/// A range of queue submissions.
		struct _queue_submission_range {
			/// No initialization.
			_queue_submission_range(uninitialized_t) {
			}
			/// Initializes all fields of this range.
			constexpr _queue_submission_range(queue_submission_index b, queue_submission_index e) :
				begin(b), end(e) {
			}
			/// Returns a range with only the given command.
			constexpr static _queue_submission_range only(queue_submission_index i) {
				return _queue_submission_range(i, i);
			}

			queue_submission_index begin; ///< The first command within the range, *inclusive*.
			queue_submission_index end; ///< The last command within the range, *inclusive*.
		};
		/// Information about a dependency acquisition event.
		struct _dependency_acquisition {
		public:
			/// No initialization.
			_dependency_acquisition(uninitialized_t) {
			}
			/// Initializes this event from another command.
			static _dependency_acquisition from_command_index(std::uint32_t q, queue_submission_index qi) {
				return _dependency_acquisition(q, qi);
			}
			/// Initializes this event from an explicit semaphore value.
			static _dependency_acquisition from_timestamp(std::uint32_t q, gpu::timeline_semaphore::value_type v) {
				return _dependency_acquisition(q, v);
			}

			std::uint32_t queue_index; ///< The queue to wait for.
			/// A dependency can either be from an explicit semaphore value, or from a command in the same batch.
			std::variant<queue_submission_index, gpu::timeline_semaphore::value_type> target;
		private:
			/// Initializes all fields of this struct.
			template <typename T> _dependency_acquisition(std::uint32_t q, T target) :
				queue_index(q),
				target(std::in_place_type<T>, target) {
			}
		};
		/// Operations that are needed for a specific command.
		struct _command_operations {
			/// All dependencies to acquire before executing the command.
			std::vector<_dependency_acquisition> acquire_dependency_requests;

			// scratch data used for dependency analysis
			/// Dependencies that need to be acquired from each queue.
			short_vector<std::variant<
				std::nullopt_t, gpu::timeline_semaphore::value_type, queue_submission_index
			>, 4> acquire_dependencies;
			/// Whether this command needs to release a dependency.
			bool release_dependency = false;
		};

		batch_context &_batch_ctx; ///< The associated \ref batch_context.
		queue_context &_queue_ctx; ///< \ref queue_context associated with the same queue.
		_details::queue_data &_q; ///< The associated command queue.
		
		/// Index of the current command that is being pseudo-executed.
		queue_submission_index _pseudo_cmd_index = zero;

		std::vector<_command_operations> _cmd_ops; ///< Operations needed for all commands.


		/// Returns a \ref _queue_submission_range corresponding to the command that's currently being executed.
		[[nodiscard]] _queue_submission_range _pseudo_execution_current_command_range() const {
			return _queue_submission_range::only(_pseudo_cmd_index);
		}

		/// Calls \p std::unreachable().
		void _pseudo_execute(const commands::invalid&) {
			std::unreachable();
		}
		/// Does nothing.
		void _pseudo_execute(const commands::start_of_batch&) {
		}
		/// Tracks usages of the source and destination buffers.
		void _pseudo_execute(const commands::copy_buffer&);
		/// Tracks usages of the source buffer and the destination image.
		void _pseudo_execute(const commands::copy_buffer_to_image&);
		/// Tracks usages of the input and output buffers.
		void _pseudo_execute(const commands::build_blas&);
		/// Tracks usages of the input and output buffers.
		void _pseudo_execute(const commands::build_tlas&);
		/// Manually handles all commands in this pass.
		void _pseudo_execute(const commands::begin_pass&);
		/// Calls \p std::unreachable() - pseudo-execution for pass commands are handled manually during
		/// \ref commands::begin_pass.
		void _pseudo_execute(const commands::draw_instanced&) {
			std::unreachable();
		}
		/// Calls \p std::unreachable() - pseudo-execution for pass commands are handled manually.
		void _pseudo_execute(const commands::end_pass&) {
			std::unreachable();
		}
		/// Tracks usages of all resources used in the compute command.
		void _pseudo_execute(const commands::dispatch_compute&);
		/// Tracks usages of all resources used in the raytrace command.
		void _pseudo_execute(const commands::trace_rays&);
		/// \todo
		void _pseudo_execute(const commands::present&);
		/// Tracks the dependency release event.
		void _pseudo_execute(const commands::release_dependency&);
		/// Tracks the dependency acquire event.
		void _pseudo_execute(const commands::acquire_dependency&);
		/// Timers are not relevant during pseudo-execution.
		void _pseudo_execute(const commands::start_timer&);
		/// Timers are not relevant during pseudo-execution.
		void _pseudo_execute(const commands::end_timer&);
		/// Pauses for the debugger.
		void _pseudo_execute(const commands::pause_for_debugging&) {
			pause_for_debugger();
		}

		/// Emulates using the given resource set to gather the necessary transitions and dependencies.
		void _pseudo_use_resource_sets(
			_details::numbered_bindings_view, gpu::synchronization_point_mask, _queue_submission_range scope
		);

		/// Emulates usages for a list of numbered bindings.
		void _pseudo_use_resource_set(
			std::span<const all_resource_bindings::numbered_binding>,
			gpu::synchronization_point_mask,
			_queue_submission_range scope
		);
		/// Emulates usages for an image descriptor array.
		void _pseudo_use_resource_set(
			recorded_resources::image_descriptor_array,
			gpu::synchronization_point_mask,
			_queue_submission_range scope
		);
		/// Emulates usages for a buffer descriptor array.
		void _pseudo_use_resource_set(
			recorded_resources::buffer_descriptor_array,
			gpu::synchronization_point_mask,
			_queue_submission_range scope
		);
		/// Emulates usages for a cached descriptor set.
		void _pseudo_use_resource_set(
			recorded_resources::cached_descriptor_set,
			gpu::synchronization_point_mask,
			_queue_submission_range scope
		);

		/// Emulates resource usage of a \ref descriptor_resource::image2d.
		void _pseudo_use_resource(
			const descriptor_resource::image2d&, gpu::synchronization_point_mask, _queue_submission_range scope
		);
		/// Emulates resource usage of a \ref descriptor_resource::image3d.
		void _pseudo_use_resource(
			const descriptor_resource::image3d&, gpu::synchronization_point_mask, _queue_submission_range scope
		);
		/// Emulates resource usage of a \ref recorded_resources::swap_chain.
		void _pseudo_use_resource(
			const descriptor_resource::swap_chain&, gpu::synchronization_point_mask, _queue_submission_range scope
		);
		/// Emulates resource usage of a \ref descriptor_resource::constant_buffer.
		void _pseudo_use_resource(
			const descriptor_resource::constant_buffer&,
			gpu::synchronization_point_mask,
			_queue_submission_range scope
		);
		/// Emulates resource usage of a \ref descriptor_resource::structured_buffer.
		void _pseudo_use_resource(
			const descriptor_resource::structured_buffer&,
			gpu::synchronization_point_mask,
			_queue_submission_range scope
		);
		/// Emulates resource usage of a \ref recorded_resources::tlas.
		void _pseudo_use_resource(
			const recorded_resources::tlas&, gpu::synchronization_point_mask, _queue_submission_range scope
		);
		/// Sampler states do not need to be tracked.
		void _pseudo_use_resource(
			const sampler_state&, gpu::synchronization_point_mask, _queue_submission_range
		) {
			// nothing to do
		}

		/// Emulates the usage of a buffer resource. Initializes the resource first if necessary.
		void _pseudo_use_buffer(
			_details::buffer&, gpu::synchronization_point_mask, gpu::buffer_access_mask, _queue_submission_range scope
		);
		/// Emulates the usage of a 2D image. Initializes the resource first if necessary.
		void _pseudo_use_image(_details::image2d&, _details::image_access, _queue_submission_range scope);
		/// Emulates the usage of a 3D image. Initializes the resource first if necessary.
		void _pseudo_use_image(_details::image3d&, _details::image_access, _queue_submission_range scope);
		/// Emulates the usage of an image resource. Initializes the resource first if necessary.
		void _pseudo_use_image_impl(_details::image_base&, _details::image_access, _queue_submission_range scope);
		/// Emulates the usage of the current image of a swap chain.
		void _pseudo_use_swap_chain(_details::swap_chain&, _details::image_access, _queue_submission_range scope);

		/// Ensures that a fresh timestamp is present, and returns its index within \ref timestamp_command_indices.
		std::uint32_t _maybe_insert_timestamp();

		/// Returns the total number of queues.
		[[nodiscard]] std::uint32_t _get_num_queues() const;
		/// Returns the index of the associated queue.
		[[nodiscard]] std::uint32_t _get_queue_index() const;
		/// Returns the queue resolve data associated with this queue.
		[[nodiscard]] batch_resolve_data::queue_data &_get_queue_resolve_data();
	};
}
