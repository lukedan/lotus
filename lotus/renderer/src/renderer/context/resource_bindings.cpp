#include "lotus/renderer/context/resource_bindings.h"

/// \file
/// Implementation of resource binding related classes.

namespace lotus::renderer {
	all_resource_bindings all_resource_bindings::create_unsorted(std::vector<resource_set_binding> s) {
		all_resource_bindings result = nullptr;
		result._sets = std::move(s);
		return result;
	}

	all_resource_bindings all_resource_bindings::create_and_sort(std::vector<resource_set_binding> s) {
		all_resource_bindings result = nullptr;
		result._sets = std::move(s);
		result.maybe_consolidate();
		return result;
	}

	all_resource_bindings all_resource_bindings::assume_sorted(std::vector<resource_set_binding> s) {
		// check that these are actually sorted
		for (std::size_t i = 1; i < s.size(); ++i) {
			crash_if(s[i - 1].space >= s[i].space);
		}
		for (const auto &set : s) {
			if (std::holds_alternative<resource_set_binding::descriptors>(set.bindings)) {
				auto &ds = std::get<resource_set_binding::descriptors>(set.bindings);
				for (std::size_t i = 1; i < ds.bindings.size(); ++i) {
					crash_if(ds.bindings[i - 1].register_index >= ds.bindings[i].register_index);
				}
			}
		}

		all_resource_bindings result = nullptr;
		result._sets = std::move(s);
		result._is_consolidated = true;
		return result;
	}

	all_resource_bindings all_resource_bindings::merge(all_resource_bindings lhs, all_resource_bindings rhs) {
		// TODO maybe optimize when both sets are consolidated?
		all_resource_bindings result = lhs;
		for (auto &set : rhs._sets) {
			result._sets.emplace_back(std::move(set));
		}
		result._is_consolidated = false;
		return result;
	}

	void all_resource_bindings::append_set(resource_set_binding bindings) {
		_sets.emplace_back(std::move(bindings));
		_is_consolidated = false;
	}

	void all_resource_bindings::maybe_consolidate() {
		if (_is_consolidated) {
			return;
		}

		// sort based on register space
		std::sort(
			_sets.begin(), _sets.end(),
			[](const resource_set_binding &lhs, const resource_set_binding &rhs) {
				return lhs.space < rhs.space;
			}
		);
		// deduplicate
		auto last = _sets.begin();
		for (auto it = _sets.begin(); it != _sets.end(); ++it) {
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
		if (last != _sets.end()) {
			_sets.erase(last + 1, _sets.end());
		}
		// sort each set
		for (auto &set : _sets) {
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

		_is_consolidated = true;
	}


	/// Collects a single descriptor.
	template <typename T> static void _collect_single_descriptor(
		std::vector<resource_set_binding> &bindings, const T &desc, const gpu::shader_resource_binding &bd
	) {
		if (bindings.empty() || bindings.back().space != bd.register_space) {
			bindings.emplace_back(resource_set_binding::descriptors({}).at_space(bd.register_space));
		} else {
			crash_if(!std::holds_alternative<resource_set_binding::descriptors>(bindings.back().bindings));
		}
		std::get<resource_set_binding::descriptors>(bindings.back().bindings).bindings.emplace_back(
			desc.at_register(bd.first_register)
		);
	}
	/// Collects a descriptor array.
	template <typename T> static void _collect_descriptor_array(
		std::vector<resource_set_binding> &bindings, const T &arr, const gpu::shader_resource_binding &bd
	) {
		crash_if((!bindings.empty() && bindings.back().space == bd.register_space) || bd.first_register != 0);
		bindings.emplace_back(arr, bd.register_space);
	}

	/// Collects an image binding.
	static void _collect_binding(
		std::vector<resource_set_binding> &bindings,
		const descriptor_resource::image2d &img, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1);
		switch (img.binding_type) {
		case image_binding_type::read_only:
			crash_if(bd.type != gpu::descriptor_type::read_only_image);
			break;
		case image_binding_type::read_write:
			crash_if(bd.type != gpu::descriptor_type::read_write_image);
			break;
		default:
			std::abort();
			break;
		}
		_collect_single_descriptor(bindings, img, bd);
	}
	/// Collects a swap chain image binding.
	static void _collect_binding(
		std::vector<resource_set_binding> &bindings,
		const descriptor_resource::swap_chain_image &img, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::read_only_image);
		_collect_single_descriptor(bindings, img, bd);
	}
	/// Collects a structured buffer binding.
	static void _collect_binding(
		std::vector<resource_set_binding> &bindings,
		const descriptor_resource::structured_buffer &buf, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1);
		switch (buf.binding_type) {
		case buffer_binding_type::read_only:
			crash_if(bd.type != gpu::descriptor_type::read_only_buffer);
			break;
		case buffer_binding_type::read_write:
			crash_if(bd.type != gpu::descriptor_type::read_write_buffer);
			break;
		default:
			std::abort();
			break;
		}
		_collect_single_descriptor(bindings, buf, bd);
	}
	/// Collects a constant buffer binding.
	static void _collect_binding(
		std::vector<resource_set_binding> &bindings,
		const descriptor_resource::immediate_constant_buffer &buf, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::constant_buffer);
		_collect_single_descriptor(bindings, buf, bd);
	}
	/// Collects a TLAS binding.
	static void _collect_binding(
		std::vector<resource_set_binding> &bindings,
		const descriptor_resource::tlas &t, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::acceleration_structure);
		_collect_single_descriptor(bindings, t, bd);
	}
	/// Collects a sampler binding.
	static void _collect_binding(
		std::vector<resource_set_binding> &bindings,
		const descriptor_resource::sampler &s, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::sampler);
		_collect_single_descriptor(bindings, s, bd);
	}
	/// Collects a image descriptor array binding.
	static void _collect_binding(
		std::vector<resource_set_binding> &bindings,
		const recorded_resources::image_descriptor_array &arr, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.type != gpu::descriptor_type::read_only_image);
		_collect_descriptor_array(bindings, arr, bd);
	}
	/// Collects a buffer descriptor array binding.
	static void _collect_binding(
		std::vector<resource_set_binding> &bindings,
		const recorded_resources::buffer_descriptor_array &arr, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.type != gpu::descriptor_type::read_only_buffer);
		_collect_descriptor_array(bindings, arr, bd);
	}
	all_resource_bindings named_bindings::into_unnamed_bindings(
		std::span<const gpu::shader_reflection *const> shaders, all_resource_bindings additional_bindings
	) const {
		// index in the `bindings' array, and then the binding that has been found
		using _binding_info = std::pair<std::size_t, gpu::shader_resource_binding>;

		std::vector<resource_set_binding> result;

		// first pass - find all bindings in shader reflection
		std::vector<_binding_info> found_bindings;
		for (std::size_t i = 0; i < bindings.size(); ++i) {
			std::u8string name(bindings[i].name);
			bool found_binding = false;
			for (const auto *refl : shaders) {
				if (auto found = refl->find_resource_binding_by_name(name)) {
					if (!found_binding) {
						found_bindings.emplace_back(i, found.value());
						found_binding = true;
					} else {
						crash_if(
							found_bindings.back().second.first_register != found->first_register ||
							found_bindings.back().second.register_space != found->register_space
						);
					}
				}
			}
		}

		// sort bindings by register space
		std::sort(
			found_bindings.begin(), found_bindings.end(),
			[](const _binding_info &lhs, const _binding_info &rhs) {
				return lhs.second.register_space < rhs.second.register_space;
			}
		);

		// second pass - collect all bindings in arrays
		for (const auto &[idx, bd] : found_bindings) {
			std::visit([&](const auto &resource) {
				_collect_binding(result, resource, bd);
			}, bindings[idx].value);
		}

		return all_resource_bindings::merge(
			all_resource_bindings::create_unsorted(std::move(result)),
			std::move(additional_bindings)
		);
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
