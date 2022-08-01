#include "lotus/renderer/asset_manager.h"

/// \file
/// Implementation of the asset manager.

#include <stb_image.h>

#include "lotus/logging.h"
#include "lotus/utils/misc.h"
#include "lotus/utils/strings.h"
#include "lotus/graphics/device.h"
#include "lotus/graphics/commands.h"
#include "lotus/graphics/context.h"
#include "lotus/renderer/resources.h"

namespace lotus::renderer::assets {
	input_buffer_binding geometry::input_buffer::into_input_buffer_binding(
		const char8_t *semantic, std::uint32_t semantic_index, std::uint32_t binding_index
	) const {
		return input_buffer_binding(
			binding_index, data, offset, stride, graphics::input_buffer_rate::per_vertex,
			{ graphics::input_buffer_element(semantic, semantic_index, format, 0) }
		);
	}


	index_buffer_binding geometry::get_index_buffer_binding() const {
		return index_buffer_binding(index_buffer, 0, index_format);
	}


	[[nodiscard]] handle<texture2d> manager::get_texture2d(const identifier &id) {
		if (auto it = _textures.find(id); it != _textures.end()) {
			if (auto ptr = it->second.lock()) {
				return handle<texture2d>(std::move(ptr));
			}
		}

		// load image
		// TODO subpath is ignored
		auto [image_mem, image_size] = load_binary_file(id.path.string().c_str());
		if (!image_mem) {
			log().error<u8"Failed to open image file: {}">(id.path.string());
			return nullptr;
		}

		// create object
		texture2d tex = nullptr;

		// get image format and load
		void *loaded = nullptr;
		std::uint8_t bytes_per_channel = 1;
		{
			int width = 0;
			int height = 0;
			int original_channels = 0;
			auto type = graphics::format_properties::data_type::unknown;
			auto *stbi_mem = static_cast<const stbi_uc*>(image_mem.get());
			if (stbi_is_hdr_from_memory(stbi_mem, static_cast<int>(image_size))) {
				type = graphics::format_properties::data_type::floating_point;
				bytes_per_channel = 4;
				loaded = stbi_loadf_from_memory(
					stbi_mem, static_cast<int>(image_size), &width, &height, &original_channels, 0
				);
			} else if (stbi_is_16_bit_from_memory(stbi_mem, static_cast<int>(image_size))) {
				type = graphics::format_properties::data_type::unsigned_norm;
				bytes_per_channel = 2;
				loaded = stbi_load_16_from_memory(
					stbi_mem, static_cast<int>(image_size), &width, &height, &original_channels, 4
				);
				original_channels = 4; // TODO support 1 and 2 channel images
			} else {
				type = graphics::format_properties::data_type::unsigned_norm;
				bytes_per_channel = 1;
				loaded = stbi_load_from_memory(
					stbi_mem, static_cast<int>(image_size), &width, &height, &original_channels, 4
				);
				original_channels = 4; // TODO support 1 and 2 channel images
			}
			image_mem.reset(); // we've done loading; free the loaded image file
			std::uint8_t num_channels = original_channels == 3 ? 4 : static_cast<std::uint8_t>(original_channels);
			std::uint8_t bits_per_channel_4[4] = { 0, 0, 0, 0 };
			for (int i = 0; i < num_channels; ++i) {
				bits_per_channel_4[i] = bytes_per_channel * 8;
			}

			auto pixel_format = graphics::format_properties::find_exact_rgba(
				bits_per_channel_4[0], bits_per_channel_4[1], bits_per_channel_4[2], bits_per_channel_4[3], type
			);
			if (pixel_format == graphics::format::none) {
				log().error<u8"Failed to find appropriate pixel format for bytes per channel {}, type {}">(
					bytes_per_channel,
					static_cast<std::underlying_type_t<graphics::format_properties::data_type>>(type)
				);
			}
			// TODO mips
			tex.image = _context.request_image2d(
				id.path.u8string(), cvec2s(width, height), 1, pixel_format,
				graphics::image_usage::mask::copy_destination | graphics::image_usage::mask::read_only_texture
			);
			tex.highest_mip_loaded = 0;
		}

		// upload
		_context.upload_image(tex.image, loaded, u8"Upload image"); // TODO better label
		stbi_image_free(loaded);

		tex.descriptor_index = _allocate_descriptor_index();
		_context.write_image_descriptors(_texture2d_descriptors, tex.descriptor_index, { tex.image });

		return _register_asset(std::move(id), std::move(tex), _textures);
	}

	handle<buffer> manager::create_buffer(
		identifier id, std::span<const std::byte> data, std::uint32_t stride, graphics::buffer_usage::mask usages
	) {
		buffer buf = nullptr;

		buf.data        = _device.create_committed_buffer(
			data.size(), _context.HACK_device_memory_type_index(),
			usages | graphics::buffer_usage::mask::copy_destination
		);
		buf.byte_size   = static_cast<std::uint32_t>(data.size());
		buf.byte_stride = stride;
		buf.usages      = usages;

		// staging buffer
		auto upload_buf = _device.create_committed_buffer(
			data.size(), _context.HACK_upload_memory_type_index(), graphics::buffer_usage::mask::copy_source
		);
		void *ptr = _device.map_buffer(upload_buf, 0, 0);
		std::memcpy(ptr, data.data(), data.size());
		_device.unmap_buffer(upload_buf, 0, data.size());

		auto cmd_list = _device.create_and_start_command_list(_cmd_alloc);
		cmd_list.copy_buffer(upload_buf, 0, buf.data, 0, data.size());
		// TODO any state transitions?
		cmd_list.finish();
		_cmd_queue.submit_command_lists({ &cmd_list }, &_fence);
		_device.wait_for_fence(_fence);
		_device.reset_fence(_fence);

		return _register_asset(std::move(id), std::move(buf), _buffers);
	}

	handle<shader> manager::compile_shader_from_source(
		const std::filesystem::path &id_path,
		std::span<const std::byte> code,
		graphics::shader_stage stage,
		std::u8string_view entry_point,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		identifier id(id_path);
		id.subpath = _assemble_shader_subid(stage, entry_point, defines);

		if (auto it = _shaders.find(id); it != _shaders.end()) {
			if (auto ptr = it->second.lock()) {
				return handle<shader>(std::move(ptr));
			}
		}

		return _do_compile_shader_from_source(std::move(id), code, stage, entry_point, defines);
	}

	handle<shader> manager::compile_shader_in_filesystem(
		const std::filesystem::path &path,
		graphics::shader_stage stage,
		std::u8string_view entry_point,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		identifier id(path);
		id.subpath = _assemble_shader_subid(stage, entry_point, defines);

		if (auto it = _shaders.find(id); it != _shaders.end()) {
			if (auto ptr = it->second.lock()) {
				return handle<shader>(std::move(ptr));
			}
		}

		auto [binary, size] = load_binary_file(path.string().c_str());
		return _do_compile_shader_from_source(
			std::move(id), { static_cast<const std::byte*>(binary.get()), size },
			stage, entry_point, defines
		);
	}

	manager::manager(
		context &ctx, graphics::device &dev, graphics::command_queue &queue,
		std::filesystem::path shader_lib_path, graphics::shader_utility *shader_utils
	) :
		_device(dev), _cmd_queue(queue), _shader_utilities(shader_utils),
		_context(ctx),
		_cmd_alloc(_device.create_command_allocator()),
		_fence(dev.create_fence(graphics::synchronization_state::unset)),
		_texture2d_descriptors(ctx.request_descriptor_array(
			u8"Texture assets", graphics::descriptor_type::read_only_image, 1024
		)),
		_texture2d_descriptor_index_alloc({ 0 }),
		_shader_library_path(std::move(shader_lib_path)) {
	}

	std::u8string manager::_assemble_shader_subid(
		graphics::shader_stage stage,
		std::u8string_view entry_point,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		static constexpr enum_mapping<graphics::shader_stage, std::u8string_view> _shader_stage_names{
			std::pair(graphics::shader_stage::all,                   u8"ALL"),
			std::pair(graphics::shader_stage::vertex_shader,         u8"vs"),
			std::pair(graphics::shader_stage::geometry_shader,       u8"gs"),
			std::pair(graphics::shader_stage::pixel_shader,          u8"ps"),
			std::pair(graphics::shader_stage::compute_shader,        u8"cs"),
			std::pair(graphics::shader_stage::callable_shader,       u8"callable"),
			std::pair(graphics::shader_stage::ray_generation_shader, u8"raygen"),
			std::pair(graphics::shader_stage::intersection_shader,   u8"intersect"),
			std::pair(graphics::shader_stage::any_hit_shader,        u8"anyhit"),
			std::pair(graphics::shader_stage::closest_hit_shader,    u8"closesthit"),
			std::pair(graphics::shader_stage::miss_shader,           u8"miss"),
		};

		std::u8string subid = std::u8string(_shader_stage_names[stage]) + u8"|" + std::u8string(entry_point);
		std::vector<std::pair<std::u8string_view, std::u8string_view>> sorted_defines(defines.begin(), defines.end());
		std::sort(sorted_defines.begin(), sorted_defines.end());
		for (const auto &[def, val] : sorted_defines) {
			subid += u8"|";
			if (!val.empty()) {
				subid += std::u8string(def) + u8"=" + std::u8string(val);
			} else {
				subid += std::u8string(def);
			}
		}
		return subid;
	}

	handle<shader> manager::_do_compile_shader_from_source(
		identifier id,
		std::span<const std::byte> code,
		graphics::shader_stage stage,
		std::u8string_view entry_point,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		if (!_shader_utilities) {
			return nullptr;
		}

		auto paths = { id.path.parent_path() }; // TODO hack to make sure the shader can include files in the same directory
		auto result = _shader_utilities->compile_shader(code, stage, entry_point, paths, defines);
		if (!result.succeeded()) {
			log().error<u8"Failed to compile shader {}: {}">(
				id.path.string(), string::to_generic(result.get_compiler_output())
			);
			return nullptr;
		}

		shader res = nullptr;
		res.binary              = _device.load_shader(result.get_compiled_binary());
		res.reflection          = _shader_utilities->load_shader_reflection(result);
		res.descriptor_bindings =
			shader_descriptor_bindings::collect_from(res.reflection);
		res.descriptor_bindings.create_layouts(get_device());
		return _register_asset(std::move(id), std::move(res), _shaders);
	}
}
