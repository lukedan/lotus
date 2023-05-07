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
#include "misc.h"
#include "resources.h"

namespace lotus::renderer::commands {
	struct start_timer;
	struct end_timer;
}

namespace lotus::renderer::execution {
	/// Whether or not to collect signatures of constant buffers.
	constexpr bool collect_constant_buffer_signature = false;

	/// Result of a single timer.
	struct timer_result {
		/// Initializes this object to empty.
		timer_result(std::nullptr_t) {
		}

		std::u8string name; ///< The name of this timer.
		float duration_ms = 0.0f; ///< Duration of the timer in milliseconds.
	};

	/// Statistics about transitions.
	struct transition_statistics {
		/// Initializes all statistics to zero.
		transition_statistics(zero_t) {
		}

		std::uint32_t requested_image2d_transitions    = 0; ///< Number of 2D image transitions requested.
		std::uint32_t requested_image3d_transitions    = 0; ///< Number of 3D image transitions requested.
		std::uint32_t requested_buffer_transitions     = 0; ///< Number of buffer transitions requested.
		std::uint32_t requested_raw_buffer_transitions = 0; ///< Number of raw buffer transitions requested.

		std::uint32_t submitted_image2d_transitions    = 0; ///< Number of 2D image transitions submitted.
		std::uint32_t submitted_image3d_transitions    = 0; ///< Number of 3D image transitions submitted.
		std::uint32_t submitted_buffer_transitions     = 0; ///< Number of buffer transitions submitted.
		std::uint32_t submitted_raw_buffer_transitions = 0; ///< Number of raw buffer transitions submitted.
	};
	/// Statistics about immediate constant buffers.
	struct immediate_constant_buffer_statistsics {
		/// Initializes all statistics to zero.
		immediate_constant_buffer_statistsics(zero_t) {
		}

		std::uint32_t num_immediate_constant_buffers = 0; ///< Number of immediate constant buffer instances.
		std::uint32_t immediate_constant_buffer_size = 0; ///< Total size of all immediate constant buffers.
		/// Total size of all immediate constant buffers without padding.
		std::uint32_t immediate_constant_buffer_size_no_padding = 0;
	};
	/// Signature of a constant buffer.
	struct constant_buffer_signature {
		/// Initializes this object to zero.
		constexpr constant_buffer_signature(zero_t) {
		}

		/// Default equality comparison.
		[[nodiscard]] friend bool operator==(constant_buffer_signature, constant_buffer_signature) = default;

		std::uint32_t hash = 0; ///< Hash of the buffer's data.
		std::uint32_t size = 0; ///< Size of the buffer.
	};
}
namespace std {
	/// Hash function for \ref lotus::renderer::execution::constant_buffer_signature.
	template <> struct hash<lotus::renderer::execution::constant_buffer_signature> {
		/// The hash function.
		[[nodiscard]] constexpr std::size_t operator()(
			lotus::renderer::execution::constant_buffer_signature sig
		) const {
			return lotus::hash_combine({
				lotus::compute_hash(sig.hash),
				lotus::compute_hash(sig.size),
			});
		}
	};
}
namespace lotus::renderer::execution {
	/// Batch statistics that are available as soon as a batch has been submitted.
	struct batch_statistics_early {
		/// Initializes all statistics to zero.
		batch_statistics_early(zero_t) {
		}

		std::vector<transition_statistics> transitions; ///< Transition statistics.
		// TODO remove these
		/// Immediate constant buffer statistics.
		std::vector<immediate_constant_buffer_statistsics> immediate_constant_buffers;
		/// Constant buffer information. This is costly to gather, so it's only filled if
		/// \ref collect_constant_buffer_signature is on.
		std::unordered_map<constant_buffer_signature, std::uint32_t> constant_buffer_counts;
	};
	/// Batch statistics that are only available once a batch has finished execution.
	struct batch_statistics_late {
		/// Initializes all statistics to zero.
		batch_statistics_late(zero_t) {
		}

		std::vector<std::vector<timer_result>> timer_results; ///< Timer results for each queue.
	};

	/// Run-time data of a timer.
	struct timer_runtime_data {
		/// Initializes this data to empty.
		timer_runtime_data(std::nullptr_t) {
		}

		std::u8string name; ///< The name of this timer.
		/// Index of the timestamp that marks the beginning of this timer.
		std::uint32_t begin_timestamp = std::numeric_limits<std::uint32_t>::max();
		/// Index of the timestamp that marks the end of this timer.
		std::uint32_t end_timestamp = std::numeric_limits<std::uint32_t>::max();
	};

	/// A batch of resources.
	struct batch_resources {
		/// Batch data related to a specific queue.
		struct queue_data {
			/// Initializes this object to empty.
			queue_data(std::nullptr_t) {
			}

			gpu::timestamp_query_heap *timestamp_heap = nullptr; ///< Timestamp heap.
			std::vector<timer_runtime_data> timers; ///< Timer data.
			std::uint32_t timestamp_count = 0; ///< Number of timestamps inserted.
		};

		/// Initializes this object to empty.
		batch_resources(std::nullptr_t) {
		}
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

		std::vector<queue_data> queues; ///< Data associated with each queue.

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

	/// Structures recording resource transition operations.
	namespace transition_records {
		/// Contains information about a layout transition operation.
		template <gpu::image_type Type> struct basic_image {
			/// Initializes this structure to empty.
			basic_image(std::nullptr_t) : mip_levels(gpu::mip_levels::all()) {
			}
			/// Initializes all fields of this struct.
			basic_image(_details::image_data_t<Type> &img, gpu::mip_levels mips, _details::image_access acc) :
				target(&img), mip_levels(mips), access(acc) {
			}

			_details::image_data_t<Type> *target = nullptr; ///< The surface to transition.
			gpu::mip_levels mip_levels; ///< Mip levels to transition.
			_details::image_access access = uninitialized; ///< Access to transition to.
		};
		using image2d = basic_image<gpu::image_type::type_2d>; ///< 2D image transition.
		using image3d = basic_image<gpu::image_type::type_3d>; ///< 3D image transition.
		/// Contains information about a buffer transition operation.
		struct buffer {
			/// Initializes this structure to empty.
			buffer(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			buffer(_details::buffer &buf, _details::buffer_access acc) : target(&buf), access(acc) {
			}

			/// Default equality and inequality comparisons.
			[[nodiscard]] friend bool operator==(const buffer&, const buffer&) = default;

			_details::buffer *target = nullptr; ///< The buffer to transition.
			_details::buffer_access access = uninitialized; ///< Access to transition to.
		};
		// TODO convert this into "generic image transition"
		/// Contains information about a layout transition operation.
		struct swap_chain {
			/// Initializes this structure to empty.
			swap_chain(std::nullptr_t) {
			}
			/// Initializes all fields of this struct.
			swap_chain(_details::swap_chain &c, _details::image_access acc) :
				target(&c), access(acc) {
			}

			_details::swap_chain *target = nullptr; ///< The swap chain to transition.
			_details::image_access access = uninitialized; ///< Access to transition to.
		};
	}

	/// A buffer for all resource transition operations.
	class transition_buffer {
	public:
		/// Initializes this buffer to empty.
		transition_buffer(std::nullptr_t) : _stats(zero) {
		}

		/// Stages a 2D image transition operation, and notifies any descriptor arrays affected.
		void stage_transition(_details::image2d&, gpu::mip_levels, _details::image_access);
		/// Stages a 3D image transition operation.
		void stage_transition(_details::image3d&, gpu::mip_levels, _details::image_access);
		/// Stages a buffer transition operation.
		void stage_transition(_details::buffer&, _details::buffer_access);
		/// Stages a swap chain transition operation.
		void stage_transition(_details::swap_chain &chain, _details::image_access usage) {
			_swap_chain_transitions.emplace_back(chain, usage);
		}
		/// Stages a raw buffer transition operation. No state tracking is performed for such operations; this is
		/// only intended to be used internally when the usage of a buffer is known.
		void stage_transition(gpu::buffer &buf, _details::buffer_access from, _details::buffer_access to) {
			++_stats.requested_raw_buffer_transitions;
			std::pair<_details::buffer_access, _details::buffer_access> usage(from, to);
			auto [it, inserted] = _raw_buffer_transitions.emplace(&buf, usage);
			crash_if(!inserted && it->second != usage);
		}
		/// Stages all pending transitions from the given image descriptor array.
		void stage_all_transitions_for(_details::image_descriptor_array&);
		/// Stages all pending transitions from the given buffer descriptor array.
		void stage_all_transitions_for(_details::buffer_descriptor_array&);

		/// Collects all staged transition operations.
		[[nodiscard]] std::tuple<
			std::vector<gpu::image_barrier>, std::vector<gpu::buffer_barrier>, transition_statistics
		> collect_transitions() &&;
	private:
		/// Staged 2D image transition operations.
		std::vector<transition_records::image2d> _image2d_transitions;
		/// Staged 3D image transition operations.
		std::vector<transition_records::image3d> _image3d_transitions;
		/// Staged buffer transition operations.
		std::vector<transition_records::buffer> _buffer_transitions;
		/// Staged swap chain transition operations.
		std::vector<transition_records::swap_chain> _swap_chain_transitions;
		/// Staged raw buffer transition operations.
		std::unordered_map<
			gpu::buffer*, std::pair<_details::buffer_access, _details::buffer_access>
		> _raw_buffer_transitions;
		transition_statistics _stats; ///< Statistics.
	};

	/// Manages the execution of a series of commands on one command queue.
	class context {
	public:
		// TODO: remove this
		/// 1MB for immediate constant buffers.
		constexpr static std::size_t immediate_constant_buffer_cache_size = 1 * 1024 * 1024;

		/// Initializes this context.
		context(_details::queue_data&, batch_resources&);
		/// Default move constructor.
		context(context&&) = default;
		/// No move construction.
		context(const context&) = delete;
		/// Default move assignment.
		context &operator=(context&&) = default;
		/// No move assignment.
		context &operator=(const context&) = delete;

		/// Returns the command queue associated with this context.
		[[nodiscard]] gpu::command_queue &get_command_queue();
		/// Creates the command list if necessary, and returns the current command list.
		[[nodiscard]] gpu::command_list &get_command_list();
		/// Submits the current command list.
		///
		/// \return Whether a command list has been submitted. If not, an empty submission will have been
		///         performed with the given synchronization requirements.
		bool submit(gpu::queue_synchronization);

		/// Records the given object to be disposed of when this frame finishes.
		template <typename T> T &record(T &&obj) {
			return _resources.record(std::move(obj));
		}
		/// Creates a new buffer with the given parameters.
		[[nodiscard]] gpu::buffer &create_buffer(std::size_t size, gpu::memory_type_index, gpu::buffer_usage_mask);
		/// Creates a frame buffer with the given parameters.
		[[nodiscard]] gpu::frame_buffer &create_frame_buffer(
			std::span<const gpu::image2d_view *const> color_rts,
			const gpu::image2d_view *ds_rt,
			cvec2u32 size
		);

		/// Allocates space for an immediate constant buffer, and calls the given callback to fill it.
		///
		/// \return A reference to the allocated region.
		[[nodiscard]] gpu::constant_buffer_view stage_immediate_constant_buffer(
			memory::size_alignment, static_function<void(std::span<std::byte>)> fill_buffer
		);
		/// \overload
		[[nodiscard]] gpu::constant_buffer_view stage_immediate_constant_buffer(
			std::span<const std::byte> data, std::size_t alignment = 0
		);
		/// Flushes all staged immediate constant buffers.
		void flush_immediate_constant_buffers();

		/// Flushes all writes to the given image descriptor array, waiting if necessary.
		void flush_descriptor_array_writes(
			_details::image_descriptor_array&, const gpu::descriptor_set_layout&
		);
		/// Flushes all writes to the given buffer descriptor array, waiting if necessary.
		void flush_descriptor_array_writes(
			_details::buffer_descriptor_array&, const gpu::descriptor_set_layout&
		);

		/// Flushes all staged transitions.
		void flush_transitions();

		/// Signals that a command has invalidated any previous timers.
		void invalidate_timer();
		/// Starts a timer.
		void start_timer(const commands::start_timer&);
		/// Ends a timer.
		void end_timer(const commands::end_timer&);

		transition_buffer transitions; ///< Transitions.
		batch_statistics_early statistics; ///< Accumulated statistics.
	private:
		_details::queue_data &_q; ///< The associated command queue.
		batch_resources &_resources; ///< Where to record internal resources created during execution to.

		gpu::command_allocator *_cmd_alloc = nullptr; ///< Command allocator used by this queue.
		gpu::command_list *_list = nullptr; ///< Current command list.
		// TODO remove this
		gpu::command_allocator *_transient_cmd_alloc = nullptr; ///< Transient command list allocator.

		bool _fresh_timestamp = false; ///< Whether we've just inserted a timestamp.

		/// Amount used in \ref _immediate_constant_device_buffer.
		std::size_t _immediate_constant_buffer_used = 0;
		/// Buffer containing all immediate constant buffers, located on the device memory.
		gpu::buffer _immediate_constant_device_buffer;
		/// Upload buffer for \ref _immediate_constant_device_buffer.
		gpu::buffer _immediate_constant_upload_buffer;
		/// Mapped pointer for \ref _immediate_constant_upload_buffer.
		std::byte *_immediate_constant_upload_buffer_ptr = nullptr;
		/// Immediate constant buffer statistics for the current chunk.
		immediate_constant_buffer_statistsics _immediate_constant_buffer_stats;


		/// Returns the associated context.
		[[nodiscard]] renderer::context &_get_context() const;
		/// Returns the associated device.
		[[nodiscard]] gpu::device &_get_device() const;
		/// Returns the batch data for the associated queue.
		[[nodiscard]] batch_resources::queue_data &_get_queue_data() const;
		// TODO remove this
		/// Returns the transient command allocator.
		[[nodiscard]] gpu::command_allocator &_get_transient_command_allocator();

		/// Inserts a timestamp if necessary, and returns its index.
		std::uint32_t _maybe_insert_timestamp();
	};
}
