#include "lotus/renderer/context/resource_bindings.h"

/// \file
/// Implementation of resource binding related classes.

namespace lotus::renderer::_details {
	/// Collects a single descriptor.
	template <typename T> static void _collect_single_descriptor(
		numbered_bindings &bindings, T &&desc, const gpu::shader_resource_binding &bd
	) {
		if (bindings.empty() || bindings.back().register_space != bd.register_space) {
			bindings.emplace_back(bd.register_space, all_resource_bindings::numbered_descriptor_bindings());
		} else {
			crash_if(
				!std::holds_alternative<all_resource_bindings::numbered_descriptor_bindings>(bindings.back().value)
			);
		}
		std::get<all_resource_bindings::numbered_descriptor_bindings>(bindings.back().value).emplace_back(
			bd.first_register, std::forward<T>(desc)
		);
	}
	/// Collects a descriptor array.
	template <typename T> static void _collect_descriptor_array(
		numbered_bindings &bindings, T &&arr, const gpu::shader_resource_binding &bd
	) {
		crash_if(
			(!bindings.empty() && bindings.back().register_space == bd.register_space) ||
			bd.first_register != 0
		);
		bindings.emplace_back(bd.register_space, std::forward<T>(arr));
	}

	/// Collects an image binding.
	template <gpu::image_type Type> static void _collect_binding(
		numbered_bindings &bindings,
		const descriptor_resource::basic_image<Type> &img, const gpu::shader_resource_binding &bd
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
		numbered_bindings &bindings,
		const recorded_resources::swap_chain &img, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::read_only_image);
		_collect_single_descriptor(bindings, img, bd);
	}
	/// Collects a constant buffer binding.
	static void _collect_binding(
		numbered_bindings &bindings,
		const descriptor_resource::constant_buffer &buf, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::constant_buffer);
		_collect_single_descriptor(bindings, buf, bd);
	}
	/// Collects a structured buffer binding.
	static void _collect_binding(
		numbered_bindings &bindings,
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
		numbered_bindings &bindings,
		descriptor_resource::immediate_constant_buffer buf, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::constant_buffer);
		_collect_single_descriptor(bindings, std::move(buf), bd);
	}
	/// Collects a TLAS binding.
	static void _collect_binding(
		numbered_bindings &bindings,
		const recorded_resources::tlas &t, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::acceleration_structure);
		_collect_single_descriptor(bindings, t, bd);
	}
	/// Collects a sampler binding.
	static void _collect_binding(
		numbered_bindings &bindings,
		const sampler_state &s, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.register_count != 1 || bd.type != gpu::descriptor_type::sampler);
		_collect_single_descriptor(bindings, s, bd);
	}
	/// Collects a image descriptor array binding.
	static void _collect_binding(
		numbered_bindings &bindings,
		const recorded_resources::image_descriptor_array &arr, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.type != gpu::descriptor_type::read_only_image);
		_collect_descriptor_array(bindings, arr, bd);
	}
	/// Collects a buffer descriptor array binding.
	static void _collect_binding(
		numbered_bindings &bindings,
		const recorded_resources::buffer_descriptor_array &arr, const gpu::shader_resource_binding &bd
	) {
		crash_if(bd.type != gpu::descriptor_type::read_only_buffer);
		_collect_descriptor_array(bindings, arr, bd);
	}
	void bindings_builder::add(
		std::vector<all_resource_bindings::named_binding> bindings,
		std::span<const gpu::shader_reflection *const> shaders
	) {
		// index in the `bindings' array, and then the binding that has been found
		using _binding_info = std::pair<std::size_t, gpu::shader_resource_binding>;

		numbered_bindings result;

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

		add(std::move(result));
	}

	numbered_bindings bindings_builder::sort_and_take() {
		using _binding_t = all_resource_bindings::numbered_binding;
		using _set_bindings_t = all_resource_bindings::numbered_set_binding;
		using _bindigns_descriptors_t = all_resource_bindings::numbered_descriptor_bindings;

		// sort based on register space
		std::sort(
			_sets.begin(), _sets.end(),
			[](const _set_bindings_t &lhs, const _set_bindings_t &rhs) {
				return lhs.register_space < rhs.register_space;
			}
		);

		// merge sets
		auto last = _sets.begin();
		for (auto it = _sets.begin(); it != _sets.end(); ++it) {
			if (last->register_space == it->register_space) {
				if (last != it) {
					if (
						std::holds_alternative<_bindigns_descriptors_t>(last->value) &&
						std::holds_alternative<_bindigns_descriptors_t>(it->value)
					) {
						auto &prev_descriptors = std::get<_bindigns_descriptors_t>(last->value);
						auto &cur_descriptors = std::get<_bindigns_descriptors_t>(it->value);
						prev_descriptors.insert(
							prev_descriptors.end(),
							std::make_move_iterator(cur_descriptors.begin()),
							std::make_move_iterator(cur_descriptors.end())
						);
					} else {
						log().error(
							"Multiple incompatible bindings specified for space {}. Only the first one will be kept",
							it->register_space
						);
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
			if (std::holds_alternative<_bindigns_descriptors_t>(set.value)) {
				auto &descriptors = std::get<_bindigns_descriptors_t>(set.value);
				std::sort(
					descriptors.begin(), descriptors.end(),
					[](const _binding_t &lhs, const _binding_t &rhs) {
						return lhs.register_index < rhs.register_index;
					}
				);
				// check for any duplicate bindings
				for (std::size_t i = 1; i < descriptors.size(); ++i) {
					if (descriptors[i].register_index == descriptors[i - 1].register_index) {
						log().error(
							"Duplicate bindings for set {} register {}",
							set.register_space, descriptors[i].register_index
						);
					}
				}
			}
		}

		return std::exchange(_sets, {});
	}
}
