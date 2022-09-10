#include "lotus/renderer/asset_manager.h"

/// \file
/// Implementation of the asset manager.

#include <stb_image.h>

#include "lotus/logging.h"
#include "lotus/utils/misc.h"
#include "lotus/utils/strings.h"
#include "lotus/gpu/device.h"
#include "lotus/gpu/commands.h"
#include "lotus/gpu/context.h"
#include "lotus/renderer/resources.h"
#include "lotus/renderer/mipmap.h"

namespace lotus::renderer::assets {
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
			auto type = gpu::format_properties::data_type::unknown;
			auto *stbi_mem = static_cast<const stbi_uc*>(image_mem.get());
			if (stbi_is_hdr_from_memory(stbi_mem, static_cast<int>(image_size))) {
				type = gpu::format_properties::data_type::floating_point;
				bytes_per_channel = 4;
				loaded = stbi_loadf_from_memory(
					stbi_mem, static_cast<int>(image_size), &width, &height, &original_channels, 0
				);
			} else if (stbi_is_16_bit_from_memory(stbi_mem, static_cast<int>(image_size))) {
				type = gpu::format_properties::data_type::unsigned_norm;
				bytes_per_channel = 2;
				loaded = stbi_load_16_from_memory(
					stbi_mem, static_cast<int>(image_size), &width, &height, &original_channels, 4
				);
				original_channels = 4; // TODO support 1 and 2 channel images
			} else {
				type = gpu::format_properties::data_type::unsigned_norm;
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

			auto pixel_format = gpu::format_properties::find_exact_rgba(
				bits_per_channel_4[0], bits_per_channel_4[1], bits_per_channel_4[2], bits_per_channel_4[3], type
			);
			if (pixel_format == gpu::format::none) {
				log().error<u8"Failed to find appropriate pixel format for bytes per channel {}, type {}">(
					bytes_per_channel,
					static_cast<std::underlying_type_t<gpu::format_properties::data_type>>(type)
				);
			}
			// TODO mips
			cvec2s size(width, height);
			tex.image = _context.request_image2d(
				id.path.u8string(), size, mipmap::get_levels(size.into<std::uint32_t>()), pixel_format,
				gpu::image_usage::mask::copy_destination |
				gpu::image_usage::mask::read_write_color_texture |
				gpu::image_usage::mask::read_only_texture
			);
			tex.highest_mip_loaded = 0;
		}

		// upload
		_context.upload_image(tex.image, loaded, u8"Upload image"); // TODO better label
		stbi_image_free(loaded);

		tex.descriptor_index = _allocate_descriptor_index();
		_context.write_image_descriptors(_texture2d_descriptors, tex.descriptor_index, { tex.image });
		return _register_asset(id, std::move(tex), _textures);
	}

	handle<buffer> manager::create_buffer(
		identifier id, std::span<const std::byte> data, gpu::buffer_usage::mask usages
	) {
		buffer buf = nullptr;
		buf.data        = _context.request_buffer(
			id.path.u8string() + id.subpath,
			static_cast<std::uint32_t>(data.size()),
			usages | gpu::buffer_usage::mask::copy_destination
		);
		_context.upload_buffer(buf.data, data, 0, u8"Load buffer asset");
		return _register_asset(std::move(id), std::move(buf), _buffers);
	}

	handle<shader> manager::compile_shader_from_source(
		const std::filesystem::path &id_path,
		std::span<const std::byte> code,
		gpu::shader_stage stage,
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
		gpu::shader_stage stage,
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

	handle<shader_library> manager::compile_shader_library_from_source(
		const std::filesystem::path &path,
		std::span<const std::byte> code,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		identifier id(path);
		id.subpath = _assemble_shader_library_subid(defines);

		if (auto it = _shader_libraries.find(id); it != _shader_libraries.end()) {
			if (auto ptr = it->second.lock()) {
				return handle<shader_library>(std::move(ptr));
			}
		}

		return _do_compile_shader_library_from_source(std::move(id), code, defines);
	}

	handle<shader_library> manager::compile_shader_library_in_filesystem(
		const std::filesystem::path &path,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		identifier id(path);
		id.subpath = _assemble_shader_library_subid(defines);

		if (auto it = _shader_libraries.find(id); it != _shader_libraries.end()) {
			if (auto ptr = it->second.lock()) {
				return handle<shader_library>(std::move(ptr));
			}
		}

		auto [binary, size] = load_binary_file(path.string().c_str());
		return _do_compile_shader_library_from_source(
			std::move(id), { static_cast<const std::byte*>(binary.get()), size }, defines
		);
	}

	manager::manager(
		context &ctx, gpu::device &dev,
		std::filesystem::path shader_lib_path, gpu::shader_utility *shader_utils
	) :
		_device(dev), _shader_utilities(shader_utils), _context(ctx),
		_texture2d_descriptors(ctx.request_descriptor_array(
			u8"Texture assets", gpu::descriptor_type::read_only_image, 1024
		)),
		_invalid_texture(nullptr),
		_texture2d_descriptor_index_alloc({ 0 }),
		_shader_library_path(std::move(shader_lib_path)) {

		{ // create "invalid" texture
			constexpr cvec2s size = cvec2s(128, 128);
			{
				texture2d tex = nullptr;
				tex.image = _context.request_image2d(
					u8"Invalid", size, 1, gpu::format::b8g8r8a8_unorm,
					gpu::image_usage::mask::copy_destination | gpu::image_usage::mask::read_only_texture
				);
				tex.descriptor_index = _allocate_descriptor_index();
				tex.highest_mip_loaded = 0;
				_context.write_image_descriptors(_texture2d_descriptors, tex.descriptor_index, { tex.image });
				_invalid_texture = _register_asset(assets::identifier({}, u8"invalid"), std::move(tex), _textures);
			}

			std::vector<linear_rgba_u8> tex_data(size[0] * size[1], zero);
			for (std::size_t y = 0; y < size[1]; ++y) {
				for (std::size_t x = 0; x < size[0]; ++x) {
					tex_data[y * size[1] + x] =
						(x ^ y) & 1 ? linear_rgba_u8(255, 0, 255, 255) : linear_rgba_u8(0, 255, 0, 255);
				}
			}
			_context.upload_image(_invalid_texture->image, tex_data.data(), u8"Invalid");
		}
	}

	std::u8string manager::_assemble_shader_subid(
		gpu::shader_stage stage,
		std::u8string_view entry_point,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		static constexpr enum_mapping<gpu::shader_stage, std::u8string_view> _shader_stage_names{
			std::pair(gpu::shader_stage::all,                   u8"ALL"),
			std::pair(gpu::shader_stage::vertex_shader,         u8"vs"),
			std::pair(gpu::shader_stage::geometry_shader,       u8"gs"),
			std::pair(gpu::shader_stage::pixel_shader,          u8"ps"),
			std::pair(gpu::shader_stage::compute_shader,        u8"cs"),
			std::pair(gpu::shader_stage::callable_shader,       u8"CALLABLE"),
			std::pair(gpu::shader_stage::ray_generation_shader, u8"RAYGEN"),
			std::pair(gpu::shader_stage::intersection_shader,   u8"INTERSECT"),
			std::pair(gpu::shader_stage::any_hit_shader,        u8"ANYHIT"),
			std::pair(gpu::shader_stage::closest_hit_shader,    u8"CLOSESTHIT"),
			std::pair(gpu::shader_stage::miss_shader,           u8"MISS"),
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

	std::u8string manager::_assemble_shader_library_subid(
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		std::u8string subid = u8"lib";
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
		gpu::shader_stage stage,
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
		auto binary = result.get_compiled_binary();
		res.binary              = _device.load_shader(binary);
		res.reflection          = _shader_utilities->load_shader_reflection(binary);
		return _register_asset(std::move(id), std::move(res), _shaders);
	}

	handle<shader_library> manager::_do_compile_shader_library_from_source(
		identifier id,
		std::span<const std::byte> code,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		if (!_shader_utilities) {
			return nullptr;
		}

		auto paths = { id.path.parent_path() }; // TODO hack to make sure the shader can include files in the same directory
		auto result = _shader_utilities->compile_shader_library(code, paths, defines);
		if (!result.succeeded()) {
			log().error<u8"Failed to compile shader {}: {}">(
				id.path.string(), string::to_generic(result.get_compiler_output())
			);
			return nullptr;
		}

		shader_library res = nullptr;
		auto binary = result.get_compiled_binary();
		res.binary              = _device.load_shader(binary);
		res.reflection          = _shader_utilities->load_shader_library_reflection(binary);
		return _register_asset(std::move(id), std::move(res), _shader_libraries);
	}
}
