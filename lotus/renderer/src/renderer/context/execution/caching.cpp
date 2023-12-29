#include "lotus/renderer/context/execution/caching.h"

/// \file
/// Implementation of caching functionalities

namespace lotus::renderer::execution {
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
					log().error(
						"Detected overlapping descriptor ranges [{}, {}] and [{}, {}]",
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
	size_t hash<lotus::renderer::execution::cache_keys::descriptor_set_layout>::operator()(
		const lotus::renderer::execution::cache_keys::descriptor_set_layout &key
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


	size_t hash<lotus::renderer::execution::cache_keys::pipeline_resources>::operator()(
		const lotus::renderer::execution::cache_keys::pipeline_resources &key
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


	size_t hash<lotus::renderer::execution::cache_keys::graphics_pipeline>::operator()(
		const lotus::renderer::execution::cache_keys::graphics_pipeline &key
	) const {
		size_t result = lotus::compute_hash(key.pipeline_rsrc);
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


	size_t hash<lotus::renderer::execution::cache_keys::raytracing_pipeline>::operator()(
		const lotus::renderer::execution::cache_keys::raytracing_pipeline &key
	) const {
		size_t result = lotus::compute_hash(key.pipeline_rsrc);
		for (const auto &hg : key.hit_group_shaders) {
			result = lotus::hash_combine(result, lotus::compute_hash(hg));
		}
		for (const auto &hg : key.hit_groups) {
			result = lotus::hash_combine(result, lotus::compute_hash(hg));
		}
		for (const auto &hg : key.general_shaders) {
			result = lotus::hash_combine(result, lotus::compute_hash(hg));
		}
		result = lotus::hash_combine({
			result,
			lotus::compute_hash(key.max_recursion_depth),
			lotus::compute_hash(key.max_payload_size),
			lotus::compute_hash(key.max_attribute_size),
		});
		return result;
	}
}


namespace lotus::renderer::execution {
	const gpu::sampler &context_cache::get_sampler(const cache_keys::sampler &key) {
		auto [it, inserted] = _samplers.try_emplace(key, nullptr);
		if (inserted) {
			it->second = _device.create_sampler(
				key.minification, key.magnification, key.mipmapping,
				key.mip_lod_bias, key.min_lod, key.max_lod, key.max_anisotropy,
				key.addressing_u, key.addressing_v, key.addressing_w,
				key.border_color, key.comparison
			);
		}
		return it->second;
	}

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
				key.sets.empty() ? 0 : key.sets.back().space + 1, &_empty_layout
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
			const auto &pipeline_rsrc = get_pipeline_resources(it->first.pipeline_rsrc);

			// gather shaders
			gpu::shader_set set = nullptr;
			set.vertex_shader = &key.vertex_shader->binary;
			set.pixel_shader  = &key.pixel_shader->binary;

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

	const gpu::raytracing_pipeline_state &context_cache::get_raytracing_pipeline_state(
		const cache_keys::raytracing_pipeline &key
	) {
		auto [it, inserted] = _raytracing_pipelines.try_emplace(key, nullptr);
		if (inserted) {
			// pipeline resources
			const auto &pipeline_rsrc = get_pipeline_resources(it->first.pipeline_rsrc);

			// shaders
			std::vector<gpu::shader_function> hg_shaders;
			hg_shaders.reserve(it->first.hit_group_shaders.size());
			for (const auto &s : it->first.hit_group_shaders) {
				hg_shaders.emplace_back(s.shader_library->binary, s.entry_point, s.stage);
			}
			std::vector<gpu::shader_function> gen_shaders;
			gen_shaders.reserve(it->first.general_shaders.size());
			for (const auto &s : it->first.general_shaders) {
				gen_shaders.emplace_back(s.shader_library->binary, s.entry_point, s.stage);
			}

			it->second = _device.create_raytracing_pipeline_state(
				hg_shaders,
				it->first.hit_groups,
				gen_shaders,
				it->first.max_recursion_depth,
				it->first.max_payload_size,
				it->first.max_attribute_size,
				pipeline_rsrc
			);
		}
		return it->second;
	}
}
