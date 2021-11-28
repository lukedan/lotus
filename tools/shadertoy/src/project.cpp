#include "project.h"

/// \file
/// Project implementation.

project project::load(const nlohmann::json &val, const error_callback &on_error) {
	project result = nullptr;
	if (!val.is_object()) {
		on_error(u8"Project must be an object");
		return result;
	}
	if (auto passes_it = val.find("passes"); passes_it != val.end()) {
		if (passes_it->is_object()) {
			for (const auto &p : passes_it->items()) {
				if (auto pass = pass::load(p.value(), on_error)) {
					pass->pass_name = std::u8string(lotus::assume_utf8(p.key()));
					result.passes.emplace(lotus::assume_utf8(p.key()), std::move(pass.value()));
				} else {
					on_error(format_utf8(u8"Failed to load pass {}", p.key()));
				}
			}
		} else {
			on_error(u8"Passes must be a JSON object");
		}
	} else {
		on_error(u8"No passes specified");
	}
	if (auto main_pass_it = val.find("main_pass"); main_pass_it != val.end()) {
		if (main_pass_it->is_string()) {
			result.main_pass = std::u8string(lotus::assume_utf8(main_pass_it->get<std::string>()));
		} else {
			on_error(u8"Invalid main pass");
		}
	} else {
		on_error(u8"No main pass specified");
	}
	return result;
}

void project::load_resources(
	lgfx::device &dev, lgfx::shader_utility &shader_utils,
	lgfx::command_allocator &cmd_alloc, lgfx::command_queue &cmd_queue,
	lgfx::shader &vert_shader, lgfx::descriptor_set_layout &global_descriptors,
	const std::filesystem::path &root, const error_callback &on_error
) {
	for (auto &it : passes) {
		it.second.load_input_images(dev, cmd_alloc, cmd_queue, root, on_error);
		it.second.load_shader(dev, shader_utils, vert_shader, global_descriptors, root, on_error);
	}
}

void project::update_descriptor_sets(
	lgfx::device &dev, lgfx::descriptor_pool &desc_pool, const error_callback &on_error
) {
	for (std::size_t i = 0; i < 2; ++i) {
		for (auto &it : passes) {
			if (it.second.shader_loaded) {
				it.second.dependencies[i].clear();
				auto descriptors = dev.create_descriptor_set(desc_pool, it.second.descriptor_set_layout);
				bool all_good = true;
				for (auto &in : it.second.inputs) {
					if (in.register_index) {
						auto &&[img_view, dep] = std::visit(
							[&](auto &val) {
								return get_image_view(val, i, on_error);
							},
							in.value
						);
						dev.write_descriptor_set_images(
							descriptors, it.second.descriptor_set_layout, in.register_index.value(), { &img_view }
						);
						if (dep) {
							it.second.dependencies[i].emplace_back(dep);
						}
					} else {
						all_good = false;
					}
				}
				it.second.input_descriptors[i] = std::move(descriptors);
				it.second.descriptors_ready = all_good;
			}
		}
	}
}

[[nodiscard]] pass::output::target *project::find_target(
	std::u8string_view name, std::size_t index, const error_callback &on_error
) {
	// first attempt: raw name
	if (auto it = passes.find(name); it != passes.end()) {
		if (!it->second.shader_loaded) {
			return nullptr;
		}
		if (it->second.output_names.size() != 1) {
			on_error(format_utf8(u8"Ambiguous output name: {}, using first output", as_string(name)));
		}
		if (!it->second.output_created) {
			on_error(format_utf8(u8"No output images created for pass {}", as_string(name)));
			return nullptr;
		}
		return &it->second.outputs[index].targets[0];
	}
	// second attempt: dot-separated
	std::size_t dot = 0;
	for (; dot < name.size(); ++dot) {
		if (name[dot] == u8'.') {
			break;
		}
	}
	if (dot < name.size()) {
		std::u8string_view pass_name(name.begin(), name.begin() + dot);
		if (auto it = passes.find(pass_name); it != passes.end()) {
			if (!it->second.shader_loaded) {
				return nullptr;
			}
			if (!it->second.output_created) {
				on_error(format_utf8(u8"No output images created for pass {}", as_string(name)));
				return nullptr;
			}
			// named
			std::u8string_view member(name.begin() + dot + 1, name.end());
			for (std::size_t i = 0; i < it->second.output_names.size(); ++i) {
				if (!it->second.output_names[i].empty() && it->second.output_names[i] == member) {
					return &it->second.outputs[index].targets[i];
				}
			}
			// indexed
			auto *beg = reinterpret_cast<const char*>(member.data());
			auto *end = beg + member.size();
			std::size_t out_index = 0;
			auto result = std::from_chars(beg, end, out_index);
			if (result.ec == std::errc() && result.ptr == end) {
				if (out_index >= it->second.output_names.size()) {
					on_error(format_utf8(
						u8"Output index {} out of range for pass {}", out_index, as_string(pass_name)
					));
					return nullptr;
				}
				return &it->second.outputs[index].targets[out_index];
			}
			on_error(format_utf8(u8"Invalid output {} for pass {}", as_string(member), as_string(pass_name)));
		}
		on_error(format_utf8(u8"Cannot find pass {}", as_string(pass_name)));
	}
	on_error(format_utf8(u8"Cannot find pass {}", as_string(name)));
	return nullptr;
}

std::pair<lgfx::image2d_view&, pass::output::target*> project::get_image_view(
	pass::input::image &img, std::size_t i, const error_callback&
) {
	return { img.texture_view, nullptr };
}

std::pair<lgfx::image2d_view&, pass::output::target*> project::get_image_view(
	pass::input::pass_output &out, std::size_t i, const error_callback &on_error
) {
	if (out.previous_frame) {
		i = (i + 1) % 2;
	}
	if (auto *target = find_target(out.name, i, on_error)) {
		return { target->image_view, target };
	}
	on_error(format_utf8(u8"Failed to find output image with key {}", as_string(out.name)));
	return { empty_image, nullptr };
}

std::vector<std::map<std::u8string, pass>::iterator> project::get_pass_order(const error_callback &on_error) {
	using _iter = std::map<std::u8string, pass>::iterator;

	std::vector<_iter> result;
	std::vector<std::pair<_iter, std::size_t>> nodes;

	for (auto it = passes.begin(); it != passes.end(); ++it) {
		std::size_t count = 0;
		for (const auto &in : it->second.inputs) {
			if (std::holds_alternative<pass::input::pass_output>(in.value)) {
				const auto &out = std::get<pass::input::pass_output>(in.value);
				if (!out.previous_frame && passes.contains(out.name)) {
					++count;
				}
			}
		}
		nodes.emplace_back(it, count);
	}

	while (!nodes.empty()) {
		_iter current = passes.end();
		for (std::size_t i = 0; i < nodes.size(); ++i) {
			if (nodes[i].second == 0) {
				current = nodes[i].first;
				std::swap(nodes[i], nodes.back());
				nodes.pop_back();
				break;
			}
		}
		if (current == passes.end()) {
			on_error(u8"Cycle detected in pass graph.");
			current = nodes.back().first;
			nodes.pop_back();
		}
		result.emplace_back(current);

		for (auto &&[it, count] : nodes) {
			for (const auto &in : it->second.inputs) {
				if (std::holds_alternative<pass::input::pass_output>(in.value)) {
					const auto &out = std::get<pass::input::pass_output>(in.value);
					if (!out.previous_frame && out.name == current->first) {
						--count;
					}
				}
			}
		}
	}

	return result;
}
