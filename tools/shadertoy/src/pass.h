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
	constexpr static lgfx::format output_image_format = lgfx::format::r32g32b32a32_float;

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
		lotus::cvec2<std::int32_t> resolution = uninitialized; ///< Screen resolution.
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
			image(std::nullptr_t) : texture(nullptr), texture_view(nullptr) {
			}

			std::filesystem::path path; ///< Path to the image.
			lgfx::image2d texture; ///< Loaded texture.
			lgfx::image2d_view texture_view; ///< Texture view.
		};

		using value_type = std::variant<pass_output, image>; ///< Input value storage type.

		/// Loads the value from the given JSON object.
		[[nodiscard]] static std::optional<value_type> load_value(const nlohmann::json&, const error_callback&);

		std::u8string binding_name; ///< Name of the texture that this is bound to.
		value_type value; ///< The value of this input.
		std::optional<std::size_t> register_index; ///< Register index of the binding.
	};
	/// A set of pass output resources.
	class output {
	public:
		/// A single image and its associated data.
		struct target {
			/// Creates an empty output target.
			target(std::nullptr_t) : image(nullptr), image_view(nullptr), current_usage(lgfx::image_usage::initial) {
			}

			/// Transitions the image to the specified state.
			void transition_to(lgfx::command_list &cmd_list, lgfx::image_usage usage) {
				if (usage != current_usage) {
					cmd_list.resource_barrier(
						{
							lgfx::image_barrier::create(
								lgfx::subresource_index::first_color(), image, current_usage, usage
							),
						},
						{}
					);
					current_usage = usage;
				}
			}

			lgfx::image2d image; ///< Output image.
			lgfx::image2d_view image_view; ///< Output image view.
			lgfx::image_usage current_usage; ///< Used for tracking the usage of this image.
		};

		/// Initializes everything to empty.
		output(std::nullptr_t) : frame_buffer(nullptr) {
		}
		/// Creates a set of output buffers.
		[[nodiscard]] static output create(
			lgfx::device&, std::size_t count, lgfx::pass_resources&, lotus::cvec2s size
		);

		std::vector<target> targets; ///< Targets.
		lgfx::frame_buffer frame_buffer; ///< Frame buffer that contains all targets.
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
		lgfx::device&, lgfx::shader_utility&, lgfx::shader &vert_shader,
		lgfx::descriptor_set_layout &global_descriptors,
		const std::filesystem::path &root, const error_callback&
	);
	/// Creates the output image and framebuffer.
	void create_output_images(lgfx::device&, lotus::cvec2s, const error_callback&);


	/// Returns whether this pass is ready to be rendered.
	[[nodiscard]] bool ready() const {
		return images_loaded && shader_loaded && output_created;
	}


	std::u8string pass_name; ///< The name of this pass.

	std::vector<input> inputs; ///< List of dependencies.
	std::vector<std::u8string> output_names; ///< Names of all outputs.
	std::filesystem::path shader_path; ///< Path to the shader file.
	std::u8string entry_point; ///< Shader entry point.

	lgfx::shader shader = nullptr; ///< The shader.
	lgfx::descriptor_set_layout descriptor_set_layout = nullptr; ///< Layout of the only descriptor set.
	lgfx::pipeline_resources pipeline_resources = nullptr; ///< Pipeline resources.
	lgfx::pass_resources pass_resources = nullptr; ///< Pass resources.
	lgfx::graphics_pipeline_state pipeline = nullptr; ///< Pipeline state.

	std::array<lgfx::descriptor_set, 2> input_descriptors{ { nullptr, nullptr} }; ///< Input descriptor sets.
	std::array<std::vector<output::target*>, 2> dependencies; ///< Output dependencies from other passes.

	std::array<output, 2> outputs{ { nullptr, nullptr } }; ///< Double-buffered output.

	bool images_loaded = false; ///< Indicates whether the images have been loaded.
	bool shader_loaded = false; ///< Indicates whether the shader and its reflection has been loaded.
	bool output_created = false; ///< Indicates whether the output images have been created.
};
