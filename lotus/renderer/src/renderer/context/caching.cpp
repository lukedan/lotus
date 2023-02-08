#include "lotus/renderer/context/caching.h"

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
		size_t result = 0;
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


	size_t hash<lotus::renderer::cache_keys::raytracing_pipeline>::operator()(
		const lotus::renderer::cache_keys::raytracing_pipeline &key
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

	size_t hash<lotus::renderer::cache_keys::shader_set_pipeline_resources>::operator()(
		const lotus::renderer::cache_keys::shader_set_pipeline_resources &key
	) const {
		size_t result = lotus::compute_hash(key.overrides);
		for (const auto &v : key.shaders) {
			result = lotus::hash_combine(result, lotus::compute_hash(v));
		}
		return result;
	}
}


namespace lotus::renderer {
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

	const context_cache::shader_set_pipeline_resources &context_cache::get_pipeline_resources(
		const cache_keys::shader_set_pipeline_resources &key,
		std::span<const gpu::shader_reflection* const> reflections
	) {
		auto [it, inserted] = _shader_pipeline_resources.try_emplace(key, nullptr);
		if (inserted) { // create pipeline resources
			std::vector<gpu::shader_resource_binding> bindings;
			for (auto *refl : reflections) {
				refl->enumerate_resource_bindings([&](const gpu::shader_resource_binding &bd) {
					bindings.emplace_back(bd);
				});
			}

			// sort
			std::sort(
				bindings.begin(), bindings.end(),
				[](const gpu::shader_resource_binding &lhs, const gpu::shader_resource_binding &rhs) {
					if (lhs.register_space == rhs.register_space) {
						return lhs.first_register < rhs.first_register;
					}
					return lhs.register_space < rhs.register_space;
				}
			);

			cache_keys::pipeline_resources rsrc;
			{ // collect into sets
				auto current_override = key.overrides.sets.begin();

				auto first = bindings.begin();
				auto last = first;
				for (; last != bindings.end(); first = last) {
					while (last != bindings.end() && last->register_space == first->register_space) {
						++last;
					}

					// check for override
					while (
						current_override != key.overrides.sets.end() &&
						current_override->space < first->register_space
					) {
						++current_override;
					}
					if (
						current_override != key.overrides.sets.end() &&
						current_override->space == first->register_space
					) {
						rsrc.sets.emplace_back(*current_override);
						continue;
					}

					// no override
					auto &set = rsrc.sets.emplace_back(nullptr, first->register_space);
					for (auto i = first; i != last; ++i) {
						set.layout.ranges.emplace_back(gpu::descriptor_range_binding::create(
							i->type, i->register_count, i->first_register
						));
					}

					using _it = std::vector<gpu::descriptor_range_binding>::const_iterator;
					set.layout.ranges.erase(
						std::unique(set.layout.ranges.begin(), set.layout.ranges.end()),
						set.layout.ranges.end()
					);
					set.layout.ranges.erase(
						gpu::descriptor_range_binding::merge_sorted_descriptor_ranges(
							set.layout.ranges.begin(), set.layout.ranges.end(),
							[](_it prev, _it next) {
								log().error<u8"Detected overlapping descriptor ranges [{}, {}] and [{}, {}]">(
									prev->register_index, prev->get_last_register_index(),
									next->register_index, next->get_last_register_index()
								);
							}
						),
						set.layout.ranges.end()
					);
				}
			}

			auto rsrc_it = _get_pipeline_resources_impl(rsrc);
			it->second.key = &rsrc_it->first;
			it->second.value = &rsrc_it->second;

			for (const auto &s : it->second.key->sets) {
				it->second.layouts.emplace_back(&get_descriptor_set_layout(s.layout));
			}
		}
		return it->second;
	}

	const context_cache::shader_set_pipeline_resources &context_cache::get_pipeline_resources(
		std::span<const assets::handle<assets::shader>> shaders,
		cache_keys::pipeline_resources overrides
	) {
		cache_keys::shader_set_pipeline_resources key = nullptr;
		key.overrides = std::move(overrides);

		std::vector<const gpu::shader_reflection*> refl;
		for (const auto &s : shaders) {
			key.shaders.emplace_back(s.get_unique_id());
			refl.emplace_back(&s->reflection);
		}
		std::sort(key.shaders.begin(), key.shaders.end());
		key.shaders.erase(std::unique(key.shaders.begin(), key.shaders.end()), key.shaders.end());

		return get_pipeline_resources(key, refl);
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

	std::unordered_map<
		cache_keys::pipeline_resources, gpu::pipeline_resources
	>::const_iterator context_cache::_get_pipeline_resources_impl(
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
		return it;
	}
}
