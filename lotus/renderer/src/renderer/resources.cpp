#include "lotus/renderer/resources.h"

#include "lotus/renderer/caching.h"

namespace lotus::renderer {
	namespace recorded_resources {
		image2d_view::image2d_view(const renderer::image2d_view &view) :
			_surface(view._surface.get()), _view_format(view._view_format), _mip_levels(view._mip_levels) {
		}

		image2d_view image2d_view::highest_mip_with_warning() const {
			image2d_view result = *this;
			if (result._mip_levels.num_levels != 1) {
				if (result._surface->num_mips - result._mip_levels.minimum > 1) {
					auto num_levels =
						result._mip_levels.is_all() ?
						result._surface->num_mips - result._mip_levels.minimum :
						result._mip_levels.num_levels;
					log().error<u8"More than one ({}) mip specified for render target for texture {}">(
						num_levels, string::to_generic(result._surface->name)
					);
				}
				result._mip_levels.num_levels = 1;
			}
			return result;
		}


		buffer::buffer(const renderer::buffer &buf) : _buffer(buf._buffer.get()) {
		}


		swap_chain::swap_chain(const renderer::swap_chain &c) : _swap_chain(c._swap_chain.get()) {
		}


		descriptor_array::descriptor_array(const renderer::descriptor_array &arr) : _array(arr._array.get()) {
		}
	}


	namespace _details {
		gpu::descriptor_type to_descriptor_type(image_binding_type type) {
			constexpr static enum_mapping<image_binding_type, gpu::descriptor_type> table{
				std::pair(image_binding_type::read_only,  gpu::descriptor_type::read_only_image ),
				std::pair(image_binding_type::read_write, gpu::descriptor_type::read_write_image),
			};
			return table[type];
		}


		cache_keys::descriptor_set_layout descriptor_array::get_layout_key() const {
			return cache_keys::descriptor_set_layout(
				{ gpu::descriptor_range_binding::create_unbounded(type, 0), },
				descriptor_set_type::variable_descriptor_count
			);
		}
	}


	namespace assets {
		input_buffer_binding geometry::input_buffer::into_input_buffer_binding(
			const char8_t *semantic, std::uint32_t semantic_index, std::uint32_t binding_index
		) const {
			return input_buffer_binding(
				binding_index, data.get().value.data, offset, stride, gpu::input_buffer_rate::per_vertex,
				{ gpu::input_buffer_element(semantic, semantic_index, format, 0) }
			);
		}
	}


	void swap_chain::resize(cvec2s sz) {
		_swap_chain->desired_size = sz;
	}


	all_resource_bindings all_resource_bindings::from_unsorted(std::vector<resource_set_binding> s) {
		all_resource_bindings result = nullptr;
		result.sets = std::move(s);
		result.consolidate();
		return result;
	}

	void all_resource_bindings::consolidate() {
		// sort based on register space
		std::sort(
			sets.begin(), sets.end(),
			[](const resource_set_binding &lhs, const resource_set_binding &rhs) {
				return lhs.space < rhs.space;
			}
		);
		// deduplicate
		auto last = sets.begin();
		for (auto it = sets.begin(); it != sets.end(); ++it) {
			if (last->space == it->space) {
				if (last != it) {
					if (
						std::holds_alternative<resource_set_binding::descriptor_bindings>(last->bindings) &&
						std::holds_alternative<resource_set_binding::descriptor_bindings>(it->bindings)
					) {
						auto &prev_descriptors = std::get<resource_set_binding::descriptor_bindings>(last->bindings);
						auto &cur_descriptors = std::get<resource_set_binding::descriptor_bindings>(it->bindings);
						for (auto &b : cur_descriptors.bindings) {
							prev_descriptors.bindings.emplace_back(std::move(b));
						}
					} else {
						log().error<
							u8"Multiple incompatible bindings specified for space {}. "
							u8"Only the first one will be kept"
						>(it->space);
					}
				}
			} else {
				++last;
				if (last != it) {
					*last = std::move(*it);
				}
			}
		}
		if (last != sets.end()) {
			sets.erase(last + 1, sets.end());
		}
		// sort each set
		for (auto &set : sets) {
			if (std::holds_alternative<resource_set_binding::descriptor_bindings>(set.bindings)) {
				auto &descriptors = std::get<resource_set_binding::descriptor_bindings>(set.bindings);
				std::sort(
					descriptors.bindings.begin(), descriptors.bindings.end(),
					[](const resource_binding &lhs, const resource_binding &rhs) {
						return lhs.register_index < rhs.register_index;
					}
				);
				// check for any duplicate bindings
				for (std::size_t i = 1; i < descriptors.bindings.size(); ++i) {
					if (descriptors.bindings[i].register_index == descriptors.bindings[i - 1].register_index) {
						log().error<
							u8"Duplicate bindings for set {} register {}"
						>(set.space, descriptors.bindings[i].register_index);
					}
				}
			}
		}
	}
}
