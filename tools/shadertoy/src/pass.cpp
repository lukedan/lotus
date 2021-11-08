#include "pass.h"

/// \file
/// Pass implementation.

#include <dxcapi.h>

#include <stb_image.h>

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
			result.name = assume_utf8(type_it->get<std::string>());
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
		result.name = assume_utf8(val.get<std::string>());
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

	// load inputs
	if (auto inputs_it = val.find("inputs"); inputs_it != val.end()) {
		if (!inputs_it->is_object()) {
			on_error(u8"Pass inputs must be an array");
		} else {
			result.inputs.reserve(inputs_it->size());
			for (auto input_desc : inputs_it.value().items()) {
				if (auto input_val = input::load_value(input_desc.value(), on_error)) {
					auto &in = result.inputs.emplace_back(uninitialized);
					in.binding_name = assume_utf8(input_desc.key());
					in.value = std::move(input_val.value());
				}
			}
		}
	}

	if (auto entry_point_it = val.find("entry_point"); entry_point_it != val.end()) {
		if (entry_point_it->is_string()) {
			result.entry_point = assume_utf8(entry_point_it->get<std::string>());
		} else {
			on_error(u8"Entry point must be a string");
		}
	} else {
		on_error(u8"No entry point specified for pass");
	}

	return result;
}

void pass::load_input_images(
	lgfx::device &dev, lgfx::command_allocator &cmd_alloc, lgfx::command_queue &cmd_queue,
	const std::filesystem::path &root, const error_callback &on_error
) {
	auto upload_fence = dev.create_fence(lgfx::synchronization_state::unset);
	for (auto &in : inputs) {
		if (std::holds_alternative<input::image>(in.value)) {
			auto &val = std::get<input::image>(in.value);
			int width = 0;
			int height = 0;
			std::filesystem::path abs_img_path = root / val.path;
			stbi_uc *data = stbi_load(abs_img_path.string().c_str(), &width, &height, nullptr, 4);
			if (!data) {
				on_error(format_utf8(u8"Failed to load image", abs_img_path.string()));
				// replace it with a default image
				width = 2;
				height = 2;
				data = new stbi_uc[]{
					255,   0, 255, 255,
					  0, 255, 255, 255,
					  0, 255, 255, 255,
					255,   0, 255, 255,
				};
			}
			val.texture = dev.create_committed_image2d(
				width, height, 1, 1, input_image_format, lgfx::image_tiling::optimal,
				lgfx::image_usage::mask::read_only_texture | lgfx::image_usage::mask::copy_destination
			);
			dev.set_debug_name(val.texture, format_utf8(u8"Static image: {}", val.path.string()));

			// copy to staging buffer
			auto buf = dev.create_committed_staging_buffer(
				width, height, input_image_format,
				lgfx::heap_type::upload, lgfx::buffer_usage::mask::copy_source
			);
			std::size_t row_length = width * 4;
			std::size_t pitch = buf.row_pitch.get_pitch_in_bytes();
			auto *ptr = reinterpret_cast<std::byte*>(dev.map_buffer(buf.data, 0, 0));
			for (std::size_t y = 0; y < height; ++y) {
				std::memcpy(ptr + pitch * y, data + row_length * y, row_length);
			}
			dev.unmap_buffer(buf.data, 0, pitch * height);

			// copy to device
			auto upload_cmd_list = dev.create_and_start_command_list(cmd_alloc);
			upload_cmd_list.resource_barrier(
				{
					lgfx::image_barrier::create(
						lgfx::subresource_index::first_color(), val.texture,
						lgfx::image_usage::initial, lgfx::image_usage::copy_destination
					),
				},
				{}
			);
			upload_cmd_list.copy_buffer_to_image(
				buf.data, 0, buf.row_pitch, lotus::aab2s::create_from_min_max(zero, lotus::cvec2s(width, height)),
				val.texture, lgfx::subresource_index::first_color(), zero
			);
			upload_cmd_list.resource_barrier(
				{
					lgfx::image_barrier::create(
						lgfx::subresource_index::first_color(), val.texture,
						lgfx::image_usage::copy_destination, lgfx::image_usage::read_only_texture
					),
				},
				{}
			);
			upload_cmd_list.finish();
			cmd_queue.submit_command_lists({ &upload_cmd_list }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);

			std::free(data);
		}
	}
	images_loaded = true;
}

void pass::load_shader(
	lgfx::device &dev, lgfx::descriptor_pool &desc_pool, lgfx::shader_utility &shader_utils,
	lgfx::shader &vert_shader, lgfx::descriptor_set_layout &global_descriptors,
	const std::filesystem::path &root, const error_callback &on_error
) {
	std::optional<lgfx::shader_reflection> reflection;

	// load shader
	std::filesystem::path abs_shader_path = root / shader_path;
	if (std::filesystem::exists(abs_shader_path)) {
		std::vector<std::byte> text = load_binary_file(abs_shader_path);
		if (!text.empty()) {
			text.insert(
				text.begin(),
				reinterpret_cast<const std::byte*>(pixel_shader_prefix.data()),
				reinterpret_cast<const std::byte*>(pixel_shader_prefix.data() + pixel_shader_prefix.size())
			);
			auto compiled = shader_utils.compile_shader(text, lgfx::shader_stage::pixel_shader, entry_point);
			auto msg = compiled.get_compiler_output();
			if (!msg.empty()) {
				on_error(msg);
			}
			if (compiled.succeeded()) {
				shader = dev.load_shader(compiled.get_compiled_binary());
				reflection = shader_utils.load_shader_reflection(compiled);
				shader_loaded = true;
			} else {
				on_error(format_utf8(u8"Failed to compile shader {}", abs_shader_path.string()));
			}
		} else {
			on_error(u8"Failed to read shader file");
		}
	}

	if (reflection) {
		// find binding register for all inputs
		for (auto &in : inputs) {
			if (auto binding = reflection->find_resource_binding_by_name(in.binding_name)) {
				in.register_index = binding->first_register;
			} else {
				on_error(format_utf8(u8"Input {} not found", as_string(in.binding_name)));
			}
		}

		std::vector<lgfx::descriptor_range_binding> bindings;
		reflection->enumerate_resource_bindings([&](const lgfx::shader_resource_binding &b) {
			if (b.register_space != 0) {
				if (b.name != u8"globals") {
					on_error(format_utf8(u8"Resource binding must be in register space 0: {}", as_string(b.name)));
				}
				return;
			}
			bindings.emplace_back(lgfx::descriptor_range_binding::create(
				b.type, b.register_count, b.first_register
			));
		});
		descriptor_set_layout = dev.create_descriptor_set_layout(bindings, lgfx::shader_stage::pixel_shader);
		descriptor_set = dev.create_descriptor_set(desc_pool, descriptor_set_layout);
		pipeline_resources = dev.create_pipeline_resources({ &descriptor_set_layout, &global_descriptors });
		pass_resources = dev.create_pass_resources(
			{
				lgfx::render_target_pass_options::create(
					output_image_format, lgfx::pass_load_operation::clear, lgfx::pass_store_operation::preserve
				)
			},
			lgfx::depth_stencil_pass_options::create(
				lgfx::format::none,
				lgfx::pass_load_operation::discard, lgfx::pass_store_operation::discard,
				lgfx::pass_load_operation::discard, lgfx::pass_store_operation::discard
			)
		);
		pipeline = dev.create_graphics_pipeline_state(
			pipeline_resources,
			lgfx::shader_set::create(vert_shader, shader),
			{ lgfx::render_target_blend_options::disabled() },
			lgfx::rasterizer_options::create(
				lgfx::depth_bias_options::disabled(),
				lgfx::front_facing_mode::clockwise, lgfx::cull_mode::none
			),
			lgfx::depth_stencil_options::all_disabled(),
			{},
			lgfx::primitive_topology::triangle_strip,
			pass_resources
		);
	}
}

void pass::create_output_image(lgfx::device &dev, lotus::cvec2s size, const error_callback &on_error) {
	output_image = nullptr;
	output_image_view = nullptr;
	frame_buffer = nullptr;

	output_image = dev.create_committed_image2d(
		size[0], size[1], 1, 1, output_image_format, lgfx::image_tiling::optimal,
		lgfx::image_usage::mask::color_render_target | lgfx::image_usage::mask::read_only_texture
	);
	output_image_view = dev.create_image2d_view_from(
		output_image, output_image_format, lgfx::mip_levels::only_highest()
	);
	frame_buffer = dev.create_frame_buffer({ &output_image_view }, nullptr, size, pass_resources);

	dev.set_debug_name(output_image, format_utf8(u8"Pass output: {}", as_string(pass_name)));

	output_created = true;
}
