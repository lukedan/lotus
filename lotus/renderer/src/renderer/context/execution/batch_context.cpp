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

	void batch_context::request_dependency(
		std::uint32_t from_queue,
		queue_submission_index from_release_before,
		std::uint32_t to_queue,
		queue_submission_index to_acquire_before
	) {
		crash_if(from_queue == to_queue);
		auto &acquired_index = _queue_pseudo_ctxs[to_queue]._pseudo_acquired_dependencies[from_queue];
		if (acquired_index != queue_submission_index::invalid && acquired_index >= from_release_before) {
			return; // the dependency has already been satisfied
		}
		acquired_index = from_release_before;
		// record this event
		if (from_release_before != queue_submission_index::zero) {
			_queue_pseudo_ctxs[from_queue]._pseudo_release_dependency_events.emplace_back(
				from_release_before, nullptr
			);
		}
		_queue_pseudo_ctxs[to_queue]._pseudo_acquire_dependency_events.emplace_back(
			from_queue, from_release_before, to_acquire_before
		);
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

	batch_resolve_data &batch_context::get_batch_resolve_data() {
		return _rctx._batch_data.back().resolve_data;
	}
}
