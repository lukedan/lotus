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
					pass->pass_name = assume_utf8(p.key());
					result.passes.emplace(assume_utf8(p.key()), std::move(pass.value()));
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
			result.main_pass = assume_utf8(main_pass_it->get<std::string>());
		} else {
			on_error(u8"Invalid main pass");
		}
	} else {
		on_error(u8"No main pass specified");
	}
	return result;
}
