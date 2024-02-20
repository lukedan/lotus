#pragma once

/// \file
/// Execution context not related to specific queues.

#include <unordered_set>

#include "lotus/renderer/context/resources.h"
#include "lotus/renderer/context/commands.h"
#include "common.h"
#include "caching.h"
#include "queue_context.h"
#include "queue_pseudo_context.h"

namespace lotus::renderer::execution {
	/// Execution-related information about a descriptor set.
	struct descriptor_set_info {
		cache_keys::descriptor_set_layout layout_key = nullptr; ///< Cache key for the descriptor set's layout.
		const gpu::descriptor_set_layout *layout = nullptr; ///< The cached layout object of this descriptor set.
		const gpu::descriptor_set *set = nullptr; ///< The descriptor set.
	};
	/// Execution-related information about all descriptor sets used in a command.
	struct pipeline_resources_info {
		cache_keys::pipeline_resources pipeline_resources_key = nullptr; ///< Cache key for the pipeline resources.
		const gpu::pipeline_resources *pipeline_resources = nullptr; ///< The cached pipeline resources object.
		std::vector<descriptor_set_binding> descriptor_sets; ///< All descriptor sets sorted by descriptor set space.
	};

	/// Execution context not related to specific queues.
	class batch_context {
	public:
		/// Initializes this batch context.
		explicit batch_context(renderer::context&);

		// pseudo-execution
		/// Inserts a dependency from after the specified command to before the current command to the specified
		/// queue.
		void request_dependency(
			std::uint32_t from_queue,
			batch_index from_batch,
			queue_submission_index from_release_after,
			std::uint32_t to_queue,
			queue_submission_index to_acquire_before
		);

		/// Marks the given swap chain as having been presented to in this batch.
		void mark_swap_chain_presented(_details::swap_chain&);


		// execution
		/// Creates a new descriptor set for the given array of bindings.
		[[nodiscard]] descriptor_set_info use_descriptor_set(
			std::span<const all_resource_bindings::numbered_binding>
		);
		/// Wraps the descriptor array in a \ref descriptor_set_info.
		[[nodiscard]] descriptor_set_info use_descriptor_set(const recorded_resources::image_descriptor_array&);
		/// Wraps the descriptor array in a \ref descriptor_set_info.
		[[nodiscard]] descriptor_set_info use_descriptor_set(const recorded_resources::buffer_descriptor_array&);
		/// Wraps the cached descriptor set in a \ref descriptor_set_info.
		[[nodiscard]] descriptor_set_info use_descriptor_set(const recorded_resources::cached_descriptor_set&);

		/// Creates a number of descriptor sets from the given bindings.
		[[nodiscard]] pipeline_resources_info use_pipeline_resources(_details::numbered_bindings_view);


		/// Retrieves a \ref queue_context.
		[[nodiscard]] queue_context &get_queue_context(std::uint32_t index) {
			return _queue_ctxs[index];
		}
		/// Retrieves a \ref queue_pseudo_context.
		[[nodiscard]] queue_pseudo_context &get_queue_pseudo_context(std::uint32_t index) {
			return _queue_pseudo_ctxs[index];
		}

		/// Finishes executing the batch.
		void finish_batch();

		/// Records a resource that is only used within this batch.
		template <typename T> T &record_batch_resource(T);
		/// Returns the batch resolve data associated with the given queue.
		[[nodiscard]] batch_resolve_data &get_batch_resolve_data();
		/// Returns the current batch index.
		[[nodiscard]] batch_index get_batch_index() const;

		/// Returns all properties of the vertex buffer of the \ref geometry_buffers_view.
		[[nodiscard]] static gpu::vertex_buffer_view get_vertex_buffer_view(const geometry_buffers_view&);
		/// Returns all properties of the index buffer of the \ref geometry_buffers_view.
		[[nodiscard]] static gpu::index_buffer_view get_index_buffer_view(const geometry_buffers_view&);
	private:
		renderer::context &_rctx; ///< The renderer context.
		short_vector<queue_pseudo_context, 4> _queue_pseudo_ctxs; ///< All pseudo-execution contexts.
		short_vector<queue_context, 4> _queue_ctxs; ///< All queue contexts.

		/// All swap chains that have been presented to during this batch.
		std::vector<_details::swap_chain*> _presented_swap_chains;
	};
}
