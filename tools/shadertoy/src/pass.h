#pragma once

/// \file
/// A shadertoy pass.

#include <fstream>
#include <filesystem>
#include <variant>

#include <nlohmann/json.hpp>

#include <lotus/common.h>
#include <lotus/utils/static_function.h>
#include <lotus/renderer/context/context.h>

#include "common.h"

/// A pass.
class pass {
public:
	/// Format of loaded input images.
	constexpr static lgpu::format input_image_format = lgpu::format::r8g8b8a8_unorm;
	/// Format of output images.
	constexpr static lgpu::format output_image_format = lgpu::format::r32g32b32a32_float;

	/// Code added before every shader.
	constexpr static std::u8string_view pixel_shader_prefix = u8R"(
		#line 100000000
		struct ps_input {
			float4 position : SV_POSITION;
			float2 uv : TEXCOORD;
		};

		struct global_input {
			float2 mouse;
			float2 mouse_down;
			float2 mouse_drag;
			int2 resolution;
			float time;
		};

		ConstantBuffer<global_input> globals : register(b0, space1);
		SamplerState nearest_sampler : register(s1, space1);
		SamplerState linear_sampler : register(s2, space1);

		#line 0
	)";

	/// Global shader input.
	struct global_input {
		/// No initialization.
		global_input(uninitialized_t) {
		}

		lotus::cvec2f mouse = uninitialized; ///< Mouse position in pixels.
		lotus::cvec2f mouse_down = uninitialized; ///< Mouse position in pixels.
		lotus::cvec2f mouse_drag = uninitialized; ///< Mouse position in pixels.
		lotus::cvec2<i32> resolution = uninitialized; ///< Screen resolution.
		float time; ///< Total run time.
	};

	/// An input.
	class input {
	public:
		/// No initialization.
		input(std::nullptr_t) {
		}

		/// Image output from another pass.
		struct pass_output {
			std::u8string name; ///< Name of the subpass.
			bool previous_frame = false; ///< Whether or not to use the previous frame's output.
		};
		/// An external image.
		struct image {
			/// Creates an empty object.
			image(std::nullptr_t) : texture(nullptr) {
			}

			std::filesystem::path path; ///< Path to the image.
			lren::assets::handle<lren::assets::image2d> texture; ///< Loaded image.
		};

		using value_type = std::variant<pass_output, image>; ///< Input value storage type.

		/// Loads the value from the given JSON object.
		[[nodiscard]] static std::optional<value_type> load_value(const nlohmann::json&);

		std::u8string binding_name; ///< Name of the texture that this is bound to.
		value_type value; ///< The value of this input.
		std::optional<u32> register_index; ///< Register index of the binding.
	};
	/// A set of pass output resources.
	struct target {
		/// Initializes this target to empty.
		target(std::nullptr_t) : previous_frame(nullptr), current_frame(nullptr) {
		}

		std::u8string name; ///< Name of this target.
		lren::image2d_view previous_frame; ///< Image of the previous frame.
		lren::image2d_view current_frame;  ///< Image of this frame.
	};

	/// Initializes everything to empty.
	pass(std::nullptr_t) {
	}
	/// Loads settings from the JSON value.
	[[nodiscard]] static std::optional<pass> load(const nlohmann::json&);

	/// Loads all input images.
	void load_input_images(
		lren::assets::manager&, const std::filesystem::path &root, const lren::pool&
	);
	/// Loads the shader and uses its reflection data to initialize the pipeline.
	void load_shader(lren::assets::manager&, const std::filesystem::path &root);


	/// Returns whether this pass is ready to be rendered.
	[[nodiscard]] bool ready() const {
		return images_loaded && shader_loaded;
	}


	std::u8string pass_name; ///< The name of this pass.

	std::filesystem::path shader_path; ///< Path to the shader file.
	std::u8string entry_point; ///< Shader entry point.
	std::vector<std::pair<std::u8string, std::u8string>> defines; ///< Defines.

	lren::assets::handle<lren::assets::shader> shader = nullptr; ///< The shader.

	std::vector<input> inputs; ///< List of dependencies.
	std::vector<target> targets; ///< Output textures of this pass.

	bool images_loaded = false; ///< Indicates whether the images have been loaded.
	bool shader_loaded = false; ///< Indicates whether the shader and its reflection has been loaded.
};
