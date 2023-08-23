#include "project.h"

/// \file
/// Project implementation.

#include "lotus/utils/strings.h"

project project::load(const nlohmann::json &val) {
	project result = nullptr;
	if (!val.is_object()) {
		log().error("Project must be an object");
		return result;
	}
	if (auto passes_it = val.find("passes"); passes_it != val.end()) {
		if (passes_it->is_object()) {
			for (const auto &p : passes_it->items()) {
				if (auto pass = pass::load(p.value())) {
					pass->pass_name = std::u8string(lotus::string::assume_utf8(p.key()));
					result.passes.emplace(lotus::string::assume_utf8(p.key()), std::move(pass.value()));
				} else {
					log().error("Failed to load pass {}", p.key());
				}
			}
		} else {
			log().error("Passes must be a JSON object");
		}
	} else {
		log().error("No passes specified");
	}
	if (auto main_pass_it = val.find("main_pass"); main_pass_it != val.end()) {
		if (main_pass_it->is_string()) {
			result.main_pass = std::u8string(lotus::string::assume_utf8(main_pass_it->get<std::string>()));
		} else {
			log().error("Invalid main pass");
		}
	} else {
		log().error("No main pass specified");
	}
	return result;
}

void project::load_resources(
	lren::assets::manager &man,
	lren::assets::handle<lren::assets::shader> vert_shader,
	const std::filesystem::path &root,
	const lren::pool &p
) {
	for (auto &it : passes) {
		it.second.load_input_images(man, root, p);
		it.second.load_shader(man, std::move(vert_shader), root);
	}
}

pass::target *project::find_target(std::u8string_view name) {
	// first attempt: raw name
	if (auto it = passes.find(name); it != passes.end()) {
		if (!it->second.shader_loaded) {
			return nullptr;
		}
		if (it->second.targets.size() != 1) {
			log().error("Ambiguous output name: {}, using first output", lstr::to_generic(name));
		}
		return &it->second.targets[0];
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
			// named
			std::u8string_view member(name.begin() + dot + 1, name.end());
			for (std::size_t i = 0; i < it->second.targets.size(); ++i) {
				if (!it->second.targets[i].name.empty() && it->second.targets[i].name == member) {
					return &it->second.targets[i];
				}
			}
			// indexed
			auto *beg = reinterpret_cast<const char*>(member.data());
			auto *end = beg + member.size();
			std::size_t out_index = 0;
			auto result = std::from_chars(beg, end, out_index);
			if (result.ec == std::errc() && result.ptr == end) {
				if (out_index >= it->second.targets.size()) {
					log().error(
						"Output index {} out of range for pass {}", out_index, lstr::to_generic(pass_name)
					);
					return nullptr;
				}
				return &it->second.targets[out_index];
			}
			log().error("Invalid output {} for pass {}", lstr::to_generic(member), lstr::to_generic(pass_name));
		}
		log().error("Cannot find pass {}", lstr::to_generic(pass_name));
	}
	log().error("Cannot find pass {}", lstr::to_generic(name));
	return nullptr;
}

std::vector<std::map<std::u8string, pass>::iterator> project::get_pass_order() {
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
			log().error("Cycle detected in pass graph.");
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
