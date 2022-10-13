#include "lotus/renderer/resource_bindings.h"

/// \file
/// Implementation of resource binding related classes.

namespace lotus::renderer {
	all_resource_bindings all_resource_bindings::from_unsorted(std::vector<resource_set_binding> s) {
		all_resource_bindings result = nullptr;
		result.sets = std::move(s);
		result.consolidate();
		return result;
	}

	all_resource_bindings all_resource_bindings::assume_sorted(std::vector<resource_set_binding> s) {
		all_resource_bindings result = nullptr;
		result.sets = std::move(s);
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
						std::holds_alternative<resource_set_binding::descriptors>(last->bindings) &&
						std::holds_alternative<resource_set_binding::descriptors>(it->bindings)
						) {
						auto &prev_descriptors = std::get<resource_set_binding::descriptors>(last->bindings);
						auto &cur_descriptors = std::get<resource_set_binding::descriptors>(it->bindings);
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
			if (std::holds_alternative<resource_set_binding::descriptors>(set.bindings)) {
				auto &descriptors = std::get<resource_set_binding::descriptors>(set.bindings);
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


namespace std {
	size_t hash<lotus::renderer::shader_function>::operator()(const lotus::renderer::shader_function &func) const {
		return lotus::hash_combine({
			lotus::compute_hash(func.shader_library),
			lotus::compute_hash(std::u8string_view(func.entry_point)),
			lotus::compute_hash(func.stage),
		});
	}
}
