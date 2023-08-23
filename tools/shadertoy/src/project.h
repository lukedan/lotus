#pragma once

/// \file
/// Shadertoy projects.

#include <map>

#include <nlohmann/json.hpp>

#include "pass.h"

/// A shadertoy project.
class project {
public:
	/// Creates an empty project.
	project(std::nullptr_t) : empty_image(nullptr) {
	}

	/// Loads a project from the given JSON object.
	[[nodiscard]] static project load(const nlohmann::json&);
	/// Loads resources for all passes.
	void load_resources(
		lren::assets::manager&,
		lren::assets::handle<lren::assets::shader> vert_shader,
		const std::filesystem::path &root,
		const lren::pool&
	);

	/// Finds the output buffer corresponding to the given name.
	[[nodiscard]] pass::target *find_target(std::u8string_view name);

	/// Returns the order in which these passes should be executed.
	[[nodiscard]] std::vector<std::map<std::u8string, pass>::iterator> get_pass_order();

	std::map<std::u8string, pass, std::less<void>> passes; ///< Passes and their names.
	std::u8string main_pass; ///< Main pass.
	lgpu::image2d_view empty_image; ///< Invalid image.
};
