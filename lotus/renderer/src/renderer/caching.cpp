#include "lotus/renderer/caching.h"

/// \file
/// Implementation of caching functionalities

namespace lotus::renderer {
	void cache_keys::descriptor_set_layout::consolidate() {
		std::sort(
			ranges.begin(), ranges.end(),
			[](
				const gpu::descriptor_range_binding &lhs,
				const gpu::descriptor_range_binding &rhs
			) {
				return lhs.register_index < rhs.register_index;
			}
		);
		// merge ranges
		using _it = decltype(ranges)::iterator;
		ranges.erase(
			gpu::descriptor_range_binding::merge_sorted_descriptor_ranges(
				ranges.begin(), ranges.end(),
				[](_it prev, _it next) {
					log().error<u8"Detected overlapping descriptor ranges [{}, {}] and [{}, {}]">(
						prev->register_index, prev->get_last_register_index(),
						next->register_index, next->get_last_register_index()
					);
				}
			),
			ranges.end()
		);
	}


	cache_keys::graphics_pipeline::input_buffer_layout::input_buffer_layout(
		const gpu::input_buffer_layout &layout
	) :
		elements(layout.elements.begin(), layout.elements.end()),
		stride(static_cast<std::uint32_t>(layout.stride)),
		buffer_index(static_cast<std::uint32_t>(layout.buffer_index)),
		input_rate(layout.input_rate) {
	}
}


namespace std {
	size_t hash<lotus::renderer::cache_keys::descriptor_set_layout>::operator()(
		const lotus::renderer::cache_keys::descriptor_set_layout &key
	) const {
		size_t result = lotus::compute_hash(key.type);
		for (const auto &r : key.ranges) {
			result = lotus::hash_combine({
				result,
				lotus::compute_hash(r.register_index),
				lotus::compute_hash(r.range.count),
				lotus::compute_hash(r.range.type),
			});
		}
		return result;
	}


	size_t hash<lotus::renderer::cache_keys::pipeline_resources>::operator()(
		const lotus::renderer::cache_keys::pipeline_resources &key
	) const {
		size_t result = 0;
		for (const auto &s : key.sets) {
			result = lotus::hash_combine({
				result,
				lotus::compute_hash(s.layout),
				lotus::compute_hash(s.space),
			});
		}
		return result;
	}


	size_t hash<lotus::renderer::cache_keys::graphics_pipeline>::operator()(
		const lotus::renderer::cache_keys::graphics_pipeline &key
	) const {
		size_t result = lotus::compute_hash(key.pipeline_resources);
		for (const auto &buf : key.input_buffers) {
			for (const auto &elem : buf.elements) {
				result = lotus::hash_combine(result, lotus::compute_hash(elem));
			}
			result = lotus::hash_combine({
				result,
				lotus::compute_hash(buf.stride),
				lotus::compute_hash(buf.buffer_index),
				lotus::compute_hash(buf.input_rate),
			});
		}
		for (const auto &fmt : key.color_rt_formats) {
			result = lotus::hash_combine(result, lotus::compute_hash(fmt));
		}
		result = lotus::hash_combine({
			result,
			lotus::compute_hash(key.dpeth_stencil_rt_format),
			lotus::compute_hash(key.vertex_shader),
			lotus::compute_hash(key.pixel_shader),
			lotus::compute_hash(key.pipeline_state),
			lotus::compute_hash(key.topology),
		});
		return result;
	}
}


namespace lotus::renderer {
	const gpu::descriptor_set_layout &context_cache::get_descriptor_set_layout(
		const cache_keys::descriptor_set_layout &key
	) {
		auto [it, inserted] = _layouts.try_emplace(key, nullptr);
		if (inserted) {
			it->second = _device.create_descriptor_set_layout(key.ranges, gpu::shader_stage::all);
		}
		return it->second;
	}

	const gpu::pipeline_resources &context_cache::get_pipeline_resources(
		const cache_keys::pipeline_resources &key
	) {
		auto [it, inserted] = _pipeline_resources.try_emplace(key, nullptr);
		if (inserted) {
			std::vector<const gpu::descriptor_set_layout*> layouts(
				key.sets.empty() ? 0 : key.sets.back().space + 1
			);
			for (const auto &set : it->first.sets) {
				layouts[set.space] = &get_descriptor_set_layout(set.layout);
			}
			it->second = _device.create_pipeline_resources(layouts);
		}
		return it->second;
	}

	const gpu::graphics_pipeline_state &context_cache::get_graphics_pipeline_state(
		const cache_keys::graphics_pipeline &key
	) {
		auto [it, inserted] = _graphics_pipelines.try_emplace(key, nullptr);
		if (inserted) {
			// pipeline resources
			const auto &pipeline_rsrc = get_pipeline_resources(it->first.pipeline_resources);

			// gather shaders
			gpu::shader_set set = nullptr;
			set.vertex_shader = &key.vertex_shader.get().value.binary;
			set.pixel_shader  = &key.pixel_shader.get().value.binary;

			// gather input buffers
			std::vector<gpu::input_buffer_layout> input_buffers;
			input_buffers.reserve(it->first.input_buffers.size());
			for (const auto &buf : it->first.input_buffers) {
				auto &gfxbuf = input_buffers.emplace_back(nullptr);
				gfxbuf.elements     = buf.elements;
				gfxbuf.stride       = buf.stride;
				gfxbuf.buffer_index = buf.buffer_index;
				gfxbuf.input_rate   = buf.input_rate;
			}

			// fb layout
			gpu::frame_buffer_layout layout(it->first.color_rt_formats, it->first.dpeth_stencil_rt_format);

			it->second = _device.create_graphics_pipeline_state(
				pipeline_rsrc,
				set,
				it->first.pipeline_state.blend_options,
				it->first.pipeline_state.rasterizer_options,
				it->first.pipeline_state.depth_stencil_options,
				input_buffers,
				it->first.topology,
				layout
			);
		}
		return it->second;
	}
}
