#include "lotus/renderer/context/execution/batch_context.h"

/// \file
/// Implementation of the non-pass-related execution context.

#include "lotus/renderer/context/context.h"
#include "lotus/renderer/context/execution/descriptors.h"
#include "lotus/renderer/context/execution/queue_pseudo_context.h"

namespace lotus::renderer::execution {
	batch_context::batch_context(renderer::context &ctx) : _rctx(ctx) {
		for (auto &q : _rctx._queues) {
			auto &qctx = _queue_ctxs.emplace_back(*this, q);
			_queue_pseudo_ctxs.emplace_back(*this, qctx, q);
		}
	}

	void batch_context::request_dependency_from_this_batch(
		std::uint32_t from_queue,
		queue_submission_index from_release_after,
		std::uint32_t to_queue,
		queue_submission_index to_acquire_before
	) {
		crash_if(std::to_underlying(from_release_after) >= _rctx._queues[from_queue].batch_commands.size());
		crash_if(std::to_underlying(to_acquire_before) >= _rctx._queues[to_queue].batch_commands.size());

		// record this event
		_queue_pseudo_ctxs[to_queue]._cmd_ops[
			std::to_underlying(to_acquire_before)
		].acquire_dependency_requests.emplace_back(
			queue_pseudo_context::_dependency_acquisition::from_command_index(from_queue, from_release_after)
		);
	}

	void batch_context::request_dependency_explicit(
		std::uint32_t from_queue,
		gpu::timeline_semaphore::value_type from_value,
		std::uint32_t to_queue,
		queue_submission_index to_acquire_before
	) {
		crash_if(std::to_underlying(to_acquire_before) >= _rctx._queues[to_queue].batch_commands.size());

		auto &to_ops = _queue_pseudo_ctxs[to_queue]._cmd_ops[std::to_underlying(to_acquire_before)];
		to_ops.acquire_dependency_requests.emplace_back(
			queue_pseudo_context::_dependency_acquisition::from_timestamp(from_queue, from_value)
		);
	}

	void batch_context::request_dependency_from_previous_batches(
		std::uint32_t from_queue,
		std::uint32_t to_queue,
		queue_submission_index to_acquire_before
	) {
		const gpu::timeline_semaphore::value_type value = get_batch_resolve_data().queues[from_queue].begin_of_batch;
		request_dependency_explicit(from_queue, value, to_queue, to_acquire_before);
	}

	void batch_context::mark_swap_chain_presented(_details::swap_chain &chain) {
		_presented_swap_chains.emplace_back(&chain);
	}

	descriptor_set_info batch_context::use_descriptor_set(
		std::span<const all_resource_bindings::numbered_binding> bindings
	) {
		descriptor_set_info result;
		result.layout_key = descriptor_set_builder::get_descriptor_set_layout_key(bindings);
		result.layout = &_rctx._cache.get_descriptor_set_layout(result.layout_key);
		descriptor_set_builder builder(_rctx, *result.layout, _rctx._descriptor_pool);
		builder.create_bindings(bindings);
		result.set = &record_batch_resource(std::move(builder).take());
		return result;
	}

	/// Wraps the descriptor array in a \ref descriptor_set_info.
	template <
		typename RecordedResource, typename View
	> [[nodiscard]] static descriptor_set_info _use_descriptor_set_array(
		const _details::descriptor_array<RecordedResource, View> &arr, context_cache &cache
	) {
		descriptor_set_info result;
		result.layout_key = cache_keys::descriptor_set_layout::for_descriptor_array(arr.type);
		result.layout = arr.layout;
		result.set = &arr.set;
		return result;
	}

	descriptor_set_info batch_context::use_descriptor_set(const recorded_resources::image_descriptor_array &arr) {
		return _use_descriptor_set_array(*arr._ptr, _rctx._cache);
	}

	descriptor_set_info batch_context::use_descriptor_set(const recorded_resources::buffer_descriptor_array &arr) {
		return _use_descriptor_set_array(*arr._ptr, _rctx._cache);
	}

	descriptor_set_info batch_context::use_descriptor_set(const recorded_resources::cached_descriptor_set &set) {
		descriptor_set_info result;
		result.layout_key = cache_keys::descriptor_set_layout(set._ptr->ranges);
		result.layout = set._ptr->layout;
		result.set = &set._ptr->set;
		return result;
	}

	pipeline_resources_info batch_context::use_pipeline_resources(_details::numbered_bindings_view sets) {
		pipeline_resources_info result;
		result.descriptor_sets.reserve(sets.size());
		result.pipeline_resources_key.sets.reserve(sets.size());
		for (const auto &set : sets) {
			auto set_info = std::visit(
				[&, this](const auto &bindings) {
					return use_descriptor_set(bindings);
				},
				set.value
			);

			result.descriptor_sets.emplace_back(*set_info.set, set.register_space);
			result.pipeline_resources_key.sets.emplace_back(std::move(set_info.layout_key), set.register_space);
		}
		std::sort(
			result.descriptor_sets.begin(), result.descriptor_sets.end(),
			[](const descriptor_set_binding &lhs, const descriptor_set_binding &rhs) {
				return lhs.space < rhs.space;
			}
		);
		result.pipeline_resources_key.sort();
		result.pipeline_resources = &_rctx._cache.get_pipeline_resources(result.pipeline_resources_key);
		return result;
	}

	void batch_context::finish_batch() {
		for (_details::swap_chain *chain : _presented_swap_chains) {
			// marked to present twice, somehow - we should already have a check somewhere else
			crash_if(chain->next_image_index == _details::swap_chain::invalid_image_index);
			chain->next_image_index = _details::swap_chain::invalid_image_index;
		}
	}

	batch_resolve_data &batch_context::get_batch_resolve_data() {
		return _rctx._batch_data.back().resolve_data;
	}

	batch_index batch_context::get_batch_index() const {
		return _rctx._batch_index;
	}

	gpu::vertex_buffer_view batch_context::get_vertex_buffer_view(const geometry_buffers_view &v) {
		return gpu::vertex_buffer_view(
			v.vertex_data._ptr->data, v.vertex_format, v.vertex_offset, v.vertex_stride, v.vertex_count
		);
	}

	gpu::index_buffer_view batch_context::get_index_buffer_view(const geometry_buffers_view &v) {
		if (!v.index_data) {
			return nullptr;
		}
		return gpu::index_buffer_view(v.index_data._ptr->data, v.index_format, v.index_offset, v.index_count);
	}
}
