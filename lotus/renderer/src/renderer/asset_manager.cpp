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

namespace lotus::renderer::assets {
	[[nodiscard]] handle<texture2d> manager::get_texture2d(const identifier &id) {
		if (auto ref = _find_asset(id, _textures)) {
			return handle<texture2d>(_textures.at(ref));
		}

		// load image
		// TODO subpath is ignored
		auto [image_mem, image_size] = load_binary_file(id.path.string().c_str());
		if (!image_mem) {
			log().error<u8"Failed to open image file: {}">(id.path.string());
			return nullptr;
		}

		// create object
		auto ref = _textures.emplace(asset<texture2d>(id, nullptr));
		auto &tex = _textures.at(ref);

		// get image format and load
		void *loaded = nullptr;
		std::uint8_t bytes_per_channel = 1;
		{
			int width = 0;
			int height = 0;
			int original_channels = 0;
			auto type = graphics::format_properties::data_type::unsigned_norm;
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
					stbi_mem, static_cast<int>(image_size), &width, &height, &original_channels, 0
				);
			} else {
				loaded = stbi_load_from_memory(
					stbi_mem, static_cast<int>(image_size), &width, &height, &original_channels, 0
				);
			}
			image_mem.reset(); // we've done loading; free the loaded image file
			std::uint8_t num_channels = original_channels == 3 ? 4 : static_cast<std::uint8_t>(original_channels);
			std::uint8_t bits_per_channel_4[4] = { 0, 0, 0, 0 };
			for (int i = 0; i < num_channels; ++i) {
				bits_per_channel_4[i] = bytes_per_channel * 8;
			}
			tex.value.pixel_format = graphics::format_properties::find_exact_rgba(
				bits_per_channel_4[0], bits_per_channel_4[1], bits_per_channel_4[2], bits_per_channel_4[3], type
			);
			if (tex.value.pixel_format == graphics::format::none) {
				log().error<u8"Failed to find appropriate pixel format for bytes per channel {}, type {}">(
					bytes_per_channel,
					static_cast<std::underlying_type_t<graphics::format_properties::data_type>>(type)
				);
			}
			tex.value.size = cvec2<std::uint32_t>(width, height);
			tex.value.num_mips = 1;
			tex.value.highest_mip_loaded = 0;
		}

		// copy to staging buffer
		auto staging_buffer = _device.create_committed_staging_buffer(
			tex.value.size[0], tex.value.size[1], tex.value.pixel_format,
			graphics::heap_type::upload, graphics::buffer_usage::mask::copy_source
		);
		auto *buf_mem = static_cast<std::byte*>(_device.map_buffer(staging_buffer.data, 0, 0));
		for (std::uint32_t y = 0; y < tex.value.size[1]; ++y) {
			auto *copy_dst = buf_mem + y * staging_buffer.row_pitch.get_pitch_in_bytes();
			auto *copy_src = static_cast<const std::byte*>(loaded) + y * tex.value.size[0];
			if (bytes_per_channel != 3) {
				for (std::uint32_t x = 0; x < tex.value.size[0]; ++x) {
					std::memcpy(
						copy_dst + bytes_per_channel * x * 4,
						copy_src + bytes_per_channel * x * 3,
						bytes_per_channel * 3
					);
				}
				for (std::uint32_t x = 0; x < tex.value.size[0]; ++x) {
					auto *dst = copy_dst + bytes_per_channel * (x * 4 + 3);
					switch (bytes_per_channel) {
					case 1:
						*reinterpret_cast<std::uint8_t*>(dst) = std::numeric_limits<std::uint8_t>::max();
						break;
					case 2:
						*reinterpret_cast<std::uint16_t*>(dst) = std::numeric_limits<std::uint16_t>::max();
						break;
					case 4:
						*reinterpret_cast<float*>(dst) = 1.0f;
						break;
					}
				}
			} else {
				std::memcpy(copy_dst, copy_src, bytes_per_channel * tex.value.size[0]);
			}
		}
		_device.unmap_buffer(staging_buffer.data, 0, staging_buffer.total_size);
		stbi_image_free(loaded);

		// copy to gpu
		tex.value.image = _device.create_committed_image2d(
			tex.value.size[0], tex.value.size[1], 1, 1, tex.value.pixel_format, graphics::image_tiling::optimal,
			graphics::image_usage::mask::read_only_texture | graphics::image_usage::mask::copy_destination
		);
		auto cmd_list = _device.create_and_start_command_list(_cmd_alloc);
		cmd_list.copy_buffer_to_image(
			staging_buffer.data, 0, staging_buffer.row_pitch,
			aab2s::create_from_min_max(zero, tex.value.size.into<std::size_t>()),
			tex.value.image, graphics::subresource_index::first_color(), zero
		);
		cmd_list.resource_barrier(
			{
				graphics::image_barrier(
					graphics::subresource_index::first_color(), tex.value.image,
					graphics::image_usage::copy_destination, graphics::image_usage::read_only_texture
				)
			}, {}
		);
		cmd_list.finish();
		_cmd_queue.submit_command_lists({ &cmd_list }, &_fence);
		_device.wait_for_fence(_fence); // TODO bad
		_device.reset_fence(_fence);

		// write descriptor
		tex.value.image_view = _device.create_image2d_view_from(
			tex.value.image, tex.value.pixel_format, graphics::mip_levels::all()
		);
		_device.write_descriptor_set_read_only_images(
			_texture2d_descriptors, _texture2d_descriptor_layout, ref.get_index(), { &tex.value.image_view }
		);

		return handle<texture2d>(tex);
	}

	handle<buffer> manager::create_buffer(
		identifier id, std::span<const std::byte> data, std::uint32_t stride, graphics::buffer_usage::mask usages
	) {
		auto ref = _buffers.emplace(asset<buffer>(std::move(id), nullptr));
		auto &buf_asset = _buffers.at(ref);

		buf_asset.value.data        = _device.create_committed_buffer(
			data.size(), graphics::heap_type::device_only,
			usages | graphics::buffer_usage::mask::copy_destination
		);
		buf_asset.value.byte_size   = data.size();
		buf_asset.value.byte_stride = stride;
		buf_asset.value.usages      = usages;

		// staging buffer
		auto upload_buf = _device.create_committed_buffer(
			data.size(), graphics::heap_type::upload, graphics::buffer_usage::mask::copy_source
		);
		void *ptr = _device.map_buffer(upload_buf, 0, 0);
		std::memcpy(ptr, data.data(), data.size());
		_device.unmap_buffer(upload_buf, 0, data.size());

		auto cmd_list = _device.create_and_start_command_list(_cmd_alloc);
		cmd_list.copy_buffer(upload_buf, 0, buf_asset.value.data, 0, data.size());
		// TODO any state transitions?
		cmd_list.finish();
		_cmd_queue.submit_command_lists({ &cmd_list }, &_fence);
		_device.wait_for_fence(_fence);
		_device.reset_fence(_fence);

		return handle<buffer>(buf_asset);
	}

	handle<shader> manager::compile_shader_from_source(const identifier &id, std::span<const std::byte> code) {
		if (auto ref = _shaders.find(
			static_cast<decltype(_shaders)::index_type>(id.hash()),
			[&id](const asset<shader> &lhs) {
				return lhs.id == id;
			}
		)) {
			return handle<shader>(_shaders.at(ref));
		}
		if (!_shader_utilities) {
			return nullptr;
		}

		auto bookmark = stack_allocator::for_this_thread().bookmark();

		auto args = bookmark.create_vector_array<std::pair<std::u8string_view, std::u8string_view>>();
		string::split<char8_t>(id.subpath, u8"|", [&args](std::u8string_view sv) {
			args.emplace_back(sv, std::u8string_view());
		});
		if (args.size() < 2) {
			log().error<u8"Insufficient number of arguments">();
			return nullptr;
		}

		graphics::shader_stage stage = graphics::shader_stage::num_enumerators;
		if (args[0].first == u8"vs") {
			stage = graphics::shader_stage::vertex_shader;
		} else if (args[0].first == u8"ps") {
			stage = graphics::shader_stage::pixel_shader;
		} else if (args[0].first == u8"cs") {
			stage = graphics::shader_stage::compute_shader;
		} else {
			// TODO implement all other stages, or report error
			assert(false);
		}

		// split definitions from values
		for (auto it = args.begin() + 2; it != args.end(); ++it) {
			std::u8string_view full = it->first;
			auto str_it = full.begin();
			for (; str_it != full.end(); ++str_it) {
				if (*str_it == u8'=') {
					break;
				}
			}
			if (str_it != full.end()) {
				it->first = std::u8string_view(full.begin(), str_it);
				it->second = std::u8string_view(str_it + 1, full.end());
			}
		}

		auto paths = { id.path.parent_path() }; // TODO hack to make sure the shader can include files in the same directory
		auto result = _shader_utilities->compile_shader(
			code, stage, args[1].first, paths, {args.begin() + 2, args.end()}
		);
		if (!result.succeeded()) {
			log().error<u8"Failed to compile shader {}: {}">(
				id.path.string(), string::to_generic(result.get_compiler_output())
			);
			return nullptr;
		}

		auto ref = _shaders.emplace(asset<shader>(id, nullptr));
		auto &shader_asset = _shaders.at(ref);
		shader_asset.value.binary              = _device.load_shader(result.get_compiled_binary());
		shader_asset.value.reflection          = _shader_utilities->load_shader_reflection(result);
		shader_asset.value.descriptor_bindings =
			shader_descriptor_bindings::collect_from(shader_asset.value.reflection);
		shader_asset.value.descriptor_bindings.create_layouts(get_device());
		return handle<shader>(shader_asset);
	}

	handle<shader> manager::compile_shader_in_filesystem(const identifier &id) {
		auto [binary, size] = load_binary_file(id.path.string().c_str());
		return compile_shader_from_source(id, { static_cast<const std::byte*>(binary.get()), size });
	}

	manager::manager(
		graphics::device &dev, graphics::command_queue &queue, graphics::shader_utility *shader_utils
	) :
		_textures(_texture_pool::create(1024)),
		_buffers(_buffer_pool::create(1024)),
		_geometries(_geometry_pool::create(1024)),
		_shaders(_shader_pool::create(1024)),
		_materials(_material_pool::create(1024)),
		_device(dev), _cmd_queue(queue), _shader_utilities(shader_utils),
		_cmd_alloc(_device.create_command_allocator()),
		_fence(dev.create_fence(graphics::synchronization_state::unset)),
		_pool(nullptr),
		_texture2d_descriptor_layout(nullptr),
		_texture2d_descriptors(nullptr) {

		// TODO magic numbers
		_pool = _device.create_descriptor_pool({
			graphics::descriptor_range::create(graphics::descriptor_type::read_only_image, 1024),
			graphics::descriptor_range::create(graphics::descriptor_type::read_only_buffer, 1024),
			graphics::descriptor_range::create(graphics::descriptor_type::acceleration_structure, 128),
		}, 1024);
		_texture2d_descriptor_layout = _device.create_descriptor_set_layout({
			graphics::descriptor_range_binding::create_unbounded(graphics::descriptor_type::read_only_image, 0),
		}, graphics::shader_stage::all);
		_texture2d_descriptors = _device.create_descriptor_set(
			_pool, _texture2d_descriptor_layout, _textures.get_pool_capacity()
		);
	}
}
