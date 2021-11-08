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
	project(std::nullptr_t) {
	}

	/// Loads a project from the given JSON object.
	[[nodiscard]] static project load(const nlohmann::json&, const error_callback&);
	/// Loads resources for all passes.
	void load_resources(
		lgfx::device &dev, lgfx::descriptor_pool &desc_pool, lgfx::shader_utility &shader_utils,
		lgfx::command_allocator &cmd_alloc, lgfx::command_queue &cmd_queue,
		lgfx::shader &vert_shader, lgfx::descriptor_set_layout &global_descriptors,
		const std::filesystem::path &root, const error_callback &on_error
	) {
		for (auto &it : passes) {
			it.second.load_input_images(dev, cmd_alloc, cmd_queue, root, on_error);
			it.second.load_shader(dev, desc_pool, shader_utils, vert_shader, global_descriptors, root, on_error);
		}
	}

	std::map<std::u8string, pass> passes; ///< Passes and their names.
	std::u8string main_pass; ///< Main pass.
};
