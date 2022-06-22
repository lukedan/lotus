#include "lotus/renderer/resources.h"

namespace lotus::renderer {
	namespace _details {
		graphics::descriptor_type to_descriptor_type(image_binding_type type) {
			constexpr static enum_mapping<image_binding_type, graphics::descriptor_type> table{
				std::pair(image_binding_type::read_only,  graphics::descriptor_type::read_only_image ),
				std::pair(image_binding_type::read_write, graphics::descriptor_type::read_write_image),
			};
			return table[type];
		}
	}


	void swap_chain::resize(cvec2s sz) {
		_swap_chain->desired_size = sz;
	}


	all_resource_bindings all_resource_bindings::from_unsorted(std::vector<resource_set_binding> s) {
		all_resource_bindings result = nullptr;
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
					for (auto &b : it->bindings) {
						last->bindings.emplace_back(std::move(b));
					}
				}
			} else {
				++last;
				if (last != it) {
					*last = std::move(*it);
				}
			}
		}
		sets.erase(last + 1, sets.end());
		// sort each set
		for (auto &set : sets) {
			std::sort(
				set.bindings.begin(), set.bindings.end(),
				[](const resource_binding &lhs, const resource_binding &rhs) {
					return lhs.register_index < rhs.register_index;
				}
			);
			// check for any duplicate bindings
			for (std::size_t i = 1; i < set.bindings.size(); ++i) {
				if (set.bindings[i].register_index == set.bindings[i - 1].register_index) {
					log().error<
						u8"Duplicate bindings for set {} register {}"
					>(set.space, set.bindings[i].register_index);
				}
			}
		}
	}
}
