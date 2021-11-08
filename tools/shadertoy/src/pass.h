#pragma once

/// \file
/// A shadertoy pass.

#include <fstream>
#include <filesystem>
#include <variant>

#include <nlohmann/json.hpp>

#include <lotus/common.h>
#include <lotus/utils/static_function.h>
#include <lotus/graphics/commands.h>
#include <lotus/graphics/context.h>
#include <lotus/graphics/device.h>
#include <lotus/graphics/resources.h>
#include <lotus/graphics/frame_buffer.h>
#include <lotus/graphics/pipeline.h>

#include "common.h"

/// A pass.
class pass {
public:
	/// Callback function for errors.
	using error_callback = lotus::static_function<void(std::u8string_view)>;

	/// Format of loaded input images.
	constexpr static lgfx::format input_image_format = lgfx::format::r8g8b8a8_srgb;
	/// Format of output images.
	constexpr static lgfx::format output_image_format = lgfx::format::r8g8b8a8_unorm;

	/// Code added before every shader.
	constexpr static std::u8string_view pixel_shader_prefix = u8R"(
	struct ps_input {
		float4 position : SV_POSITION;
		float2 uv : TEXCOORD;
	};

	struct global_input {
		float time;
		int2 resolution;
	};

	ConstantBuffer<global_input> globals : register(b0, space1);

	#line 0
	)";

	/// Global shader input.
	struct global_input {
		/// No initialization.
		global_input(uninitialized_t) {
		}

		float time; ///< Total run time.
		lotus::cvec2i resolution = uninitialized; ///< Screen resolution.
	};

	/// An input.
	class input {
	public:
		/// No initialization.
		input(uninitialized_t) {
		}

		/// Image output from another pass.
		struct pass_output {
			std::u8string name; ///< Name of the subpass.
		};
		/// An external image.
		struct image {
			/// Creates an empty object.
			image(std::nullptr_t) : texture(nullptr) {
			}

			std::filesystem::path path; ///< Path to the image.
			lgfx::image2d texture; ///< Loaded texture.
		};

		using value_type = std::variant<pass_output, image>; ///< Input value storage type.

		/// Loads the value from the given JSON object.
		[[nodiscard]] static std::optional<value_type> load_value(const nlohmann::json&, const error_callback&);

		std::u8string binding_name; ///< Name of the texture that this is bound to.
		value_type value; ///< The value of this input.
		std::size_t register_index; ///< Register index of the binding.
	};

	/// Initializes everything to empty.
	pass(std::nullptr_t) {
	}
	/// Loads settings from the JSON value.
	[[nodiscard]] static std::optional<pass> load(const nlohmann::json&, const error_callback&);

	/// Loads all input images.
	void load_input_images(
		lgfx::device&, lgfx::command_allocator&, lgfx::command_queue&,
		const std::filesystem::path &root, const error_callback&
	);
	/// Loads the shader and uses its reflection data to initialize the pipeline.
	void load_shader(
		lgfx::device&, lgfx::descriptor_pool&, lgfx::shader_utility&,
		lgfx::shader &vert_shader, lgfx::descriptor_set_layout &global_descriptors,
		const std::filesystem::path &root, const error_callback&
	);
	/// Creates the output image and framebuffer.
	void create_output_image(lgfx::device&, lotus::cvec2s, const error_callback&);


	/// Returns whether this pass is ready to be rendered.
	[[nodiscard]] bool ready() const {
		return images_loaded && shader_loaded && output_created;
	}


	std::u8string pass_name; ///< The name of this pass.

	std::vector<input> inputs; ///< List of dependencies.
	std::filesystem::path shader_path; ///< Path to the shader file.
	std::u8string entry_point; ///< Shader entry point.

	lgfx::shader shader = nullptr; ///< The shader.
	lgfx::descriptor_set_layout descriptor_set_layout = nullptr; ///< Layout of the only descriptor set.
	lgfx::descriptor_set descriptor_set = nullptr; ///< Descriptor set.
	lgfx::pipeline_resources pipeline_resources = nullptr; ///< Pipeline resources.
	lgfx::pass_resources pass_resources = nullptr; ///< Pass resources.
	lgfx::graphics_pipeline_state pipeline = nullptr; ///< Pipeline state.

	lgfx::image2d output_image = nullptr; ///< Output image.
	lgfx::image2d_view output_image_view = nullptr; ///< Output image view.
	lgfx::frame_buffer frame_buffer = nullptr; ///< Frame buffer.

	bool images_loaded = false;
	bool shader_loaded = false;
	bool output_created = false;
};
