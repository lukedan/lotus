#include "pass.h"

/// \file
/// Pass implementation.

#include <dxcapi.h>

#include <stb_image.h>

#include "lotus/utils/strings.h"
#include "lotus/utils/misc.h"
#include "lotus/renderer/asset_manager.h"

std::optional<pass::input::value_type> pass::input::load_value(
	const nlohmann::json &val, const error_callback &on_error
) {
	if (val.is_object()) {
		if (auto type_it = val.find("pass"); type_it != val.end()) {
			if (!type_it->is_string()) {
				on_error(u8"Referenced pass name must be a string");
				return std::nullopt;
			}
			input::pass_output result;
			result.name = std::u8string(lotus::string::assume_utf8(type_it->get<std::string>()));
			if (!result.name.empty() && result.name[0] == u8'-') {
				result.name = result.name.substr(1);
				result.previous_frame = true;
			}
			return result;
		}
		if (auto type_it = val.find("image"); type_it != val.end()) {
			if (!type_it->is_string()) {
				on_error(u8"External image path must be a string");
				return std::nullopt;
			}
			input::image result = nullptr;
			result.path = type_it->get<std::string>();
			return result;
		}
		on_error(u8"No valid input type found");
		return std::nullopt;
	} else if (val.is_string()) { // shorthand for a pass
		input::pass_output result;
		result.name = std::u8string(lotus::string::assume_utf8(val.get<std::string>()));
		return result;
	}
	on_error(u8"Invalid pass input format");
	return std::nullopt;
}


std::optional<pass> pass::load(const nlohmann::json &val, const error_callback &on_error) {
	if (!val.is_object()) {
		on_error(u8"Pass must be described using a JSON object");
		return std::nullopt;
	}

	auto shader_file_it = val.find("source");
	if (shader_file_it == val.end() || !shader_file_it->is_string()) {
		on_error(u8"Pass source file must be a string");
		return std::nullopt;
	}

	pass result = nullptr;

	result.shader_path = shader_file_it->get<std::string>();

	if (auto inputs_it = val.find("inputs"); inputs_it != val.end()) {
		if (!inputs_it->is_object()) {
			on_error(u8"Pass inputs must be an array");
		} else {
			result.inputs.reserve(inputs_it->size());
			for (auto input_desc : inputs_it.value().items()) {
				if (auto input_val = input::load_value(input_desc.value(), on_error)) {
					auto &in = result.inputs.emplace_back(nullptr);
					in.binding_name = std::u8string(lotus::string::assume_utf8(input_desc.key()));
					in.value = std::move(input_val.value());
				}
			}
		}
	}

	if (auto entry_point_it = val.find("entry_point"); entry_point_it != val.end()) {
		if (entry_point_it->is_string()) {
			result.entry_point = std::u8string(lotus::string::assume_utf8(entry_point_it->get<std::string>()));
		} else {
			on_error(u8"Entry point must be a string");
		}
	} else {
		result.entry_point = u8"main"; // default entry point is main. not main_ps, just main
	}

	if (auto outputs_it = val.find("outputs"); outputs_it != val.end()) {
		if (outputs_it->is_array()) {
			for (const auto &str : outputs_it.value()) {
				if (str.is_string()) {
					result.targets.emplace_back(nullptr).name = lotus::string::assume_utf8(str.get<std::string>());
				} else {
					result.targets.emplace_back(nullptr); // preserve indices
					on_error(u8"Output at location must be a string");
				}
			}
		} else if (outputs_it->is_number_unsigned()) {
			result.targets.resize(outputs_it->get<std::size_t>(), nullptr);
		} else {
			on_error(u8"Pass outputs must be a list of strings or a single integer");
		}
	}

	if (auto defines_it = val.find("defines"); defines_it != val.end()) {
		if (defines_it->is_object()) {
			for (const auto &def : defines_it.value().items()) {
				auto &pair = result.defines.emplace_back(lotus::string::assume_utf8(def.key()), u8"");
				if (def.value().is_string()) {
					pair.second = std::u8string(lotus::string::assume_utf8(def.value().get<std::string>()));
				} else if (def.value().is_number_integer()) {
					pair.second = std::u8string(
						lotus::string::assume_utf8(std::to_string(def.value().get<std::int64_t>()))
					);
				} else if (def.value().is_number()) {
					pair.second = std::u8string(
						lotus::string::assume_utf8(std::to_string(def.value().get<double>()))
					);
				} else if (!def.value().is_null()) {
					on_error(u8"Invalid define value type");
				}
			}
		} else if (defines_it->is_array()) {
			for (const auto &def : defines_it.value()) {
				if (def.is_string()) {
					result.defines.emplace_back(lotus::string::assume_utf8(def.get<std::string>()), u8"");
				} else {
					on_error(u8"Define is not a string");
				}
			}
		} else {
			on_error(u8"Invalid defines");
		}
	}

	return result;
}

void pass::load_input_images(
	lren::assets::manager &man, const std::filesystem::path &root, const error_callback &on_error
) {
	for (auto &in : inputs) {
		if (std::holds_alternative<input::image>(in.value)) {
			auto &val = std::get<input::image>(in.value);
			val.texture = man.get_texture2d(lren::assets::identifier(root / val.path));
		}
	}
	images_loaded = true;
}

void pass::load_shader(
	lren::assets::manager &man, lren::assets::handle<lren::assets::shader> vert_shader,
	const std::filesystem::path &root, const error_callback &on_error
) {
	shader_loaded = false;

	// load shader
	if (std::filesystem::path abs_shader_path = root / shader_path; std::filesystem::exists(abs_shader_path)) {
		// prepend global shader code
		auto [code, size] = lotus::load_binary_file(abs_shader_path.string().c_str());
		const auto *data1 = static_cast<const std::byte*>(static_cast<const void*>(pixel_shader_prefix.data()));
		const auto *data2 = static_cast<const std::byte*>(code.get());
		std::vector<std::byte> full_code(data1, data1 + pixel_shader_prefix.size());
		full_code.insert(full_code.end(), data2, data2 + size);
		shader = man.compile_shader_from_source(
			abs_shader_path, full_code, lgpu::shader_stage::pixel_shader, entry_point, defines
		);
	} else {
		return;
	}

	auto &reflection = shader.get().value.reflection;

	// find binding register for all inputs
	for (auto &in : inputs) {
		if (auto binding = reflection.find_resource_binding_by_name(in.binding_name)) {
			in.register_index = binding->first_register;
		} else {
			on_error(format_utf8<u8"Input {} not found">(lotus::string::to_generic(in.binding_name)));
		}
	}

	// find the number of outputs
	std::size_t num_outputs = 0;
	reflection.enumerate_output_variables([&](const lgpu::shader_output_variable &var) {
		if (var.semantic_name == u8"SV_TARGET") {
			++num_outputs;
		}
		// TODO do we need to make sure that these targets have contiguous indices?
	});
	if (!targets.empty() && targets.size() < num_outputs) {
		on_error(format_utf8<u8"Only {} output names specified, while the shader has {} outputs">(
			targets.size(), num_outputs
		));
	} else if (targets.size() > num_outputs) {
		on_error(format_utf8<
			u8"Too many output names specified for shader: got {}, expected {}. "
			u8"Out-of-bounds ones will be discarded"
		>(targets.size(), num_outputs));
	}
	targets.resize(num_outputs, nullptr);

	// check that all resources are bound in space 0
	reflection.enumerate_resource_bindings([&](const lgpu::shader_resource_binding &b) {
		if (b.register_space != 0) {
			if (b.name != u8"globals" && b.name != u8"nearest_sampler" && b.name != u8"linear_sampler") {
				on_error(format_utf8<u8"Resource binding must be in register space 0: {}">(lstr::to_generic(b.name)));
			}
		}
	});

	shader_loaded = true;
}
