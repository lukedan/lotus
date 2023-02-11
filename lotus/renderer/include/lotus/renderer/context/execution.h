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

namespace lotus::renderer {
	class context;
}

namespace lotus::renderer::execution {
	/// Whether or not to collect signatures of constant buffers.
	constexpr bool collect_constant_buffer_signature = false;

	/// Timestamp information of a timer.
	struct timestamp_data {
		/// Initializes this struct to empty.
		timestamp_data(std::nullptr_t) {
		}

		std::uint32_t first_timestamp = 0; ///< Index of the first timestamp in the heap.
		std::uint32_t second_timestamp = 0; ///< Index of the second timestamp in the heap.
		std::u8string_view name; ///< The name of this timer.
	};
	/// Result of a single timer.
	struct timer_result {
		/// Initializes this object to empty.
		timer_result(std::nullptr_t) {
		}

		std::u8string_view name; ///< The name of this timer.
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

		std::vector<timer_result> timer_results; ///< Timer results.
	};


	/// A batch of resources.
	struct batch_resources {
		/// Initializes this object to empty.
		batch_resources(std::nullptr_t) :
			graphics_cmd_alloc(nullptr), compute_cmd_alloc(nullptr), transient_cmd_alloc(nullptr),
			timestamp_heap(nullptr) {
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
		std::deque<gpu::frame_buffer>              frame_buffers;          ///< Frame buffers.
		std::deque<gpu::swap_chain>                swap_chains;            ///< Swap chains.
		std::deque<gpu::fence>                     fences;                 ///< Fences.

		std::vector<std::unique_ptr<_details::image2d>>    image2d_meta;    ///< 2D images to be disposed next frame.
		std::vector<std::unique_ptr<_details::image3d>>    image3d_meta;    ///< 3D images to be disposed next frame.
		std::vector<std::unique_ptr<_details::swap_chain>> swap_chain_meta; ///< Swap chain to be disposed next frame.
		std::vector<std::unique_ptr<_details::buffer>>     buffer_meta;     ///< Buffers to be disposed next frame.

		gpu::command_allocator graphics_cmd_alloc; ///< Graphics command allocator.
		gpu::command_allocator compute_cmd_alloc; ///< Compute command allocator.
		gpu::command_allocator transient_cmd_alloc; ///< Transient command allocator.

		gpu::timestamp_query_heap timestamp_heap; ///< Timestamp query heap for this batch.
		std::vector<timestamp_data> timestamps; ///< All recorded timers.
		std::uint32_t num_timestamps = 0; ///< Number of timestamps recorded.

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
			} else if constexpr (std::is_same_v<T, gpu::swap_chain>) {
				return swap_chains.emplace_back(std::move(obj));
			} else if constexpr (std::is_same_v<T, gpu::fence>) {
				return fences.emplace_back(std::move(obj));
			} else {
				static_assert(sizeof(T*) == 0, "Unhandled resource type");
			}
		}
	};

	/// Manages large buffers suballocated for upload operations.
	struct upload_buffers {
	public:
		constexpr static std::size_t default_buffer_size = 10 * 1024 * 1024; ///< Default size for upload buffers.

		/// Function used to allocate new buffers. The returned buffer should not be moved after it is created.
		using allocate_buffer_func = static_function<gpu::buffer&(std::size_t)>;
		/// The type of allocation performed by \ref stage().
		enum class allocation_type {
			invalid, ///< Invalid.
			/// The allocation comes from the same buffer as the previous one that's not an individual allocataion.
			same_buffer,
			/// A new buffer has been created for this allocation and maybe some following allocations, if there is
			/// enough space.
			new_buffer,
			/// The allocation is too large and a dedicated buffer is created for it.
			individual_buffer,
		};
		/// Result of 
		struct result {
			friend upload_buffers;
		public:
			gpu::buffer *buffer = nullptr; ///< Buffer that the allocation is from.
			std::uint32_t offset = 0; ///< Offset of the allocation in bytes from the start of the buffer.
			allocation_type type = allocation_type::invalid; ///< The type of this allocation.
		private:
			/// Initializes all fields of this struct.
			result(gpu::buffer &b, std::uint32_t off, allocation_type t) : buffer(&b), offset(off), type(t) {
			}
		};

		/// Initializes this object to empty.
		upload_buffers(std::nullptr_t) : allocate_buffer(nullptr), _current(nullptr) {
		}
		/// Initializes this object with the given device and parameters.
		explicit upload_buffers(
			renderer::context &ctx, allocate_buffer_func alloc, std::size_t buf_size = default_buffer_size
		) : allocate_buffer(std::move(alloc)), _current(nullptr), _buffer_size(buf_size), _context(&ctx) {
		}

		/// Allocates space for a chunk of the given size and writes the given data into it.
		[[nodiscard]] result stage(std::span<const std::byte>, std::size_t alignment);
		/// Flushes the current buffer. The caller only need to transition the buffers from CPU write to copy source.
		void flush();

		/// Returns the size of regular allocated buffers.
		[[nodiscard]] std::size_t get_buffer_size() const {
			return _buffer_size;
		}

		/// Returns whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return _context;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}

		/// Callback used by this object to allocate a new buffer.
		static_function<gpu::buffer&(std::size_t)> allocate_buffer;
	private:
		gpu::buffer *_current; ///< Current buffer that's being filled out.
		std::byte *_current_ptr = nullptr; ///< Mapped pointer of \ref _current.
		std::size_t _current_used = 0; ///< Amount used in \ref _current.

		std::size_t _buffer_size = 0; ///< Size of \ref _current or any newly allocated buffers.
		renderer::context *_context = nullptr; ///< Associated context.
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

	/// Manages the execution of a series of commands.
	class context {
	public:
		/// 1MB for immediate constant buffers.
		constexpr static std::size_t immediate_constant_buffer_cache_size = 1 * 1024 * 1024;

		/// Initializes this context.
		context(renderer::context &ctx, batch_resources &rsrc) :
			transitions(nullptr),
			statistics(zero),
			_ctx(ctx), _resources(rsrc),
			_immediate_constant_device_buffer(nullptr),
			_immediate_constant_upload_buffer(nullptr),
			_immediate_constant_buffer_stats(zero) {
		}
		/// No move construction.
		context(const context&) = delete;
		/// No move assignment.
		context &operator=(const context&) = delete;

		/// Creates the command list if necessary, and returns the current command list.
		[[nodiscard]] gpu::command_list &get_command_list();
		/// Submits the current command list.
		///
		/// \return Whether a command list has been submitted. If not, an empty submission will have been
		///         performed with the given synchronization requirements.
		bool submit(gpu::command_queue &q, gpu::queue_synchronization sync) {
			flush_immediate_constant_buffers();

			if (!_list) {
				q.submit_command_lists({}, std::move(sync));
				return false;
			}

			_list->finish();
			q.submit_command_lists({ _list }, std::move(sync));
			_list = nullptr;
			return true;
		}

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

		transition_buffer transitions; ///< Transitions.
		batch_statistics_early statistics; ///< Accumulated statistics.
	private:
		renderer::context &_ctx; ///< The associated context.
		batch_resources &_resources; ///< Where to record internal resources created during execution to.
		gpu::command_list *_list = nullptr; ///< Current command list.

		/// Amount used in \ref _immediate_constant_device_buffer.
		std::size_t _immediate_constant_buffer_used = 0;
		/// Buffer containing all immediate constant buffers, located on the device memory.
		gpu::buffer _immediate_constant_device_buffer;
		/// Upload buffer for \ref _immediate_constant_device_buffer.
		gpu::buffer _immediate_constant_upload_buffer;
		/// Mapped pointer for \ref _immediate_constant_upload_buffer.
		std::byte *_immediate_constant_upload_buffer_ptr = nullptr;
		/// Immediate constant buffer statistics.
		immediate_constant_buffer_statistsics _immediate_constant_buffer_stats;
	};
}
