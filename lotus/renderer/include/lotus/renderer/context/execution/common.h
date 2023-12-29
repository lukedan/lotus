#pragma once

/// \file
/// Common execution related classes and utilities.

#include "lotus/gpu/descriptors.h"
#include "lotus/gpu/pipeline.h"

namespace lotus::renderer::execution {
	/// Whether or not to collect signatures of constant buffers.
	constexpr bool collect_constant_buffer_signature = false;

	/// A descriptor set and its register space.
	struct descriptor_set_binding {
		/// Initializes this structure to empty.
		descriptor_set_binding(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		descriptor_set_binding(const gpu::descriptor_set &se, std::uint32_t s) : set(&se), space(s) {
		}

		const gpu::descriptor_set *set = nullptr; ///< The descriptor set.
		std::uint32_t space = 0; ///< Register space of this descriptor set.
	};

	/// Cached data used by a single pass command.
	struct pass_command_data {
		/// Initializes this structure to empty.
		pass_command_data(std::nullptr_t) {
		}

		const gpu::pipeline_resources *resources = nullptr; ///< Pipeline resources.
		const gpu::graphics_pipeline_state *pipeline_state = nullptr; ///< Pipeline state.
		std::vector<descriptor_set_binding> descriptor_sets; ///< Descriptor sets.
	};

	/// Data associated with one timer.
	struct timer_data {
		/// Initializes all values to invalid.
		timer_data(std::nullptr_t) :
			first_timestamp(std::numeric_limits<std::uint32_t>::max()),
			second_timestamp(std::numeric_limits<std::uint32_t>::max()) {
		}

		std::uint32_t first_timestamp; ///< Index of the first timestamp.
		std::uint32_t second_timestamp; ///< Index of the second timestamp.
	};

	/// A batch of resources.
	struct batch_resources {
		/// Default constructor.
		batch_resources() = default;
		/// Default move constructor.
		batch_resources(batch_resources&&) = default;
		/// Default move assignment.
		batch_resources &operator=(batch_resources&&) = default;
		/// This destructor ensures proper destruction order.
		~batch_resources() {
			// command lists must be destroyed before the command allocators
			command_lists.clear();
		}

		std::deque<gpu::descriptor_set>            descriptor_sets;        ///< Descriptor sets.
		std::deque<gpu::descriptor_set_layout>     descriptor_set_layouts; ///< Descriptor set layouts.
		std::deque<gpu::pipeline_resources>        pipeline_resources;     ///< Pipeline resources.
		std::deque<gpu::compute_pipeline_state>    compute_pipelines;      ///< Compute pipeline states.
		std::deque<gpu::graphics_pipeline_state>   graphics_pipelines;     ///< Graphics pipeline states.
		std::deque<gpu::raytracing_pipeline_state> raytracing_pipelines;   ///< Raytracing pipeline states.
		std::deque<gpu::image2d>                   images;                 ///< Images.
		std::deque<gpu::image2d_view>              image2d_views;          ///< 2D image views.
		std::deque<gpu::image3d_view>              image3d_views;          ///< 3D image views.
		std::deque<gpu::buffer>                    buffers;                ///< Constant buffers.
		std::deque<gpu::command_list>              command_lists;          ///< Command lists.
		std::deque<gpu::command_allocator>         command_allocs;         ///< Command allocators.
		std::deque<gpu::frame_buffer>              frame_buffers;          ///< Frame buffers.
		std::deque<gpu::swap_chain>                swap_chains;            ///< Swap chains.
		std::deque<gpu::fence>                     fences;                 ///< Fences.
		std::deque<gpu::timestamp_query_heap>      timestamp_heaps;        ///< Timestamp query heaps.

		std::vector<std::unique_ptr<_details::image2d>>    image2d_meta;    ///< 2D images to be disposed next frame.
		std::vector<std::unique_ptr<_details::image3d>>    image3d_meta;    ///< 3D images to be disposed next frame.
		std::vector<std::unique_ptr<_details::swap_chain>> swap_chain_meta; ///< Swap chain to be disposed next frame.
		std::vector<std::unique_ptr<_details::buffer>>     buffer_meta;     ///< Buffers to be disposed next frame.

		/// Registers the given object as a resource.
		template <typename T> T &record(T &&obj) {
			if constexpr (std::is_same_v<T, gpu::descriptor_set>) {
				return descriptor_sets.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::descriptor_set_layout>) {
				return descriptor_set_layouts.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::pipeline_resources>) {
				return pipeline_resources.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::compute_pipeline_state>) {
				return compute_pipelines.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::graphics_pipeline_state>) {
				return graphics_pipelines.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::raytracing_pipeline_state>) {
				return raytracing_pipelines.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::image2d>) {
				return images.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::image2d_view>) {
				return image2d_views.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::image3d_view>) {
				return image3d_views.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::buffer>) {
				return buffers.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::command_list>) {
				return command_lists.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::command_allocator>) {
				return command_allocs.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::frame_buffer>) {
				return frame_buffers.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::swap_chain>) {
				return swap_chains.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::fence>) {
				return fences.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::timestamp_query_heap>) {
				return timestamp_heaps.emplace_back(std::move(obj));
			} else {
				static_assert(sizeof(T*) == 0, "Unhandled resource type");
			}
		}
	};
	/// Non-resource data associated with a batch.
	struct batch_resolve_data {
		/// Data associated with a specific queue for a batch.
		struct queue_data {
			gpu::timestamp_query_heap *timestamp_heap = nullptr; ///< Timestamps.
			std::vector<timer_data> timers; ///< Data associated with all timers.
			std::uint32_t num_timestamps = 0; ///< Total number of timestamps.

			/// The value of the time stamp inserted at the very end of the previous batch on this queue.
			gpu::timeline_semaphore::value_type begin_of_batch = 0;
			/// The value of the time stamp inserted at the very end of this batch on this queue.
			gpu::timeline_semaphore::value_type end_of_batch = 0;
		};

		short_vector<queue_data, 4> queues; ///< Data associated with all queues.

		/// Index of the first command that belongs to this batch.
		global_submission_index first_command = global_submission_index::zero;
	};
	/// Data associated with a batch.
	struct batch_data {
		batch_resources resources; ///< Resources used only by this batch.
		batch_resolve_data resolve_data; ///< Data used for further execution and for generating statistics.
	};

	/// Indicates an event where a dependency needs to be released.
	template <typename Data> struct release_dependency_event {
		/// No initialization.
		release_dependency_event(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		release_dependency_event(queue_submission_index cmd_idx, Data d) : command_index(cmd_idx), data(d) {
		}

		queue_submission_index command_index; ///< The index of the command.
		Data data; ///< Associated event data.
	};
	/// Indicates an event where a dependency needs to be acquired.
	template <typename Data> struct acquire_dependency_event {
		/// No initialization.
		acquire_dependency_event(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		acquire_dependency_event(std::uint32_t q, Data d, queue_submission_index acq_idx) :
			queue_index(q), data(d), command_index(acq_idx) {
		}

		std::uint32_t queue_index; ///< The queue index to acquire this dependency from.
		Data data; ///< Associated event data.
		queue_submission_index command_index; ///< Command index of the acquire event.
	};
}
