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
	[[nodiscard]] static project load(const nlohmann::json&, const error_callback&);
	/// Loads resources for all passes.
	void load_resources(
		lgfx::device&, lgfx::shader_utility&,
		lgfx::command_allocator&, lgfx::command_queue&,
		lgfx::shader &vert_shader, lgfx::descriptor_set_layout &global_descriptors,
		const std::filesystem::path &root, const error_callback&
	);
	/// Updates descriptor sets for all passes.
	void update_descriptor_sets(lgfx::device&, lgfx::descriptor_pool&, const error_callback&);

	/// Finds the output buffer corresponding to the given name.
	[[nodiscard]] pass::output::target *find_target(
		std::u8string_view name, std::size_t index, const error_callback&
	);

	/// Returns the image view corresponding to the given input.
	[[nodiscard]] std::pair<lgfx::image2d_view&, pass::output::target*> get_image_view(
		pass::input::image&, std::size_t i, const error_callback&
	);
	/// Returns the image view corresponding to the given input.
	[[nodiscard]] std::pair<lgfx::image2d_view&, pass::output::target*> get_image_view(
		pass::input::pass_output&, std::size_t i, const error_callback&
	);

	/// Returns the order in which these passes should be executed.
	[[nodiscard]] std::vector<std::map<std::u8string, pass>::iterator> get_pass_order(const error_callback&);

	std::map<std::u8string, pass, std::less<void>> passes; ///< Passes and their names.
	std::u8string main_pass; ///< Main pass.
	lgfx::image2d_view empty_image; ///< Invalid image.
};
