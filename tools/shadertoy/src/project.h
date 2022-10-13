#pragma once

/// \file
/// Shadertoy projects.

#include <map>

#include <nlohmann/json.hpp>

#include "pass.h"

/// A shadertoy project.
class project {
public:
	/// Callback function for errors.
	using error_callback = lotus::static_function<void(std::u8string_view)>;

	/// Creates an empty project.
	project(std::nullptr_t) : empty_image(nullptr) {
	}

	/// Loads a project from the given JSON object.
	[[nodiscard]] static project load(const nlohmann::json&, error_callback&);
	/// Loads resources for all passes.
	void load_resources(
		lren::assets::manager&,
		lren::assets::handle<lren::assets::shader> vert_shader,
		const std::filesystem::path &root, error_callback&
	);

	/// Finds the output buffer corresponding to the given name.
	[[nodiscard]] pass::target *find_target(std::u8string_view name, error_callback&);

	/// Returns the order in which these passes should be executed.
	[[nodiscard]] std::vector<std::map<std::u8string, pass>::iterator> get_pass_order(error_callback&);

	std::map<std::u8string, pass, std::less<void>> passes; ///< Passes and their names.
	std::u8string main_pass; ///< Main pass.
	lgpu::image2d_view empty_image; ///< Invalid image.
};
