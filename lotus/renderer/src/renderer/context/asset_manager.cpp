#include "lotus/renderer/context/asset_manager.h"

/// \file
/// Implementation of the asset manager.

#include <stb_image.h>

#include "lotus/logging.h"
#include "lotus/utils/misc.h"
#include "lotus/utils/strings.h"
#include "lotus/gpu/device.h"
#include "lotus/gpu/commands.h"
#include "lotus/gpu/context.h"
#include "lotus/renderer/context/resources.h"
#include "lotus/renderer/loaders/dds_loader.h"
#include "lotus/renderer/mipmap.h"

namespace lotus::renderer::assets {
	manager::_async_loader::_async_loader() : _state(state::running) {
		_job_thread = std::thread([this]() {
			_job_thread_func();
		});
	}

	manager::_async_loader::~_async_loader() {
		_state = state::shutting_down;
		_signal.notify_all();
		_job_thread.join();
	}

	void manager::_async_loader::add_jobs(std::vector<job> jobs) {
		{
			std::unique_lock<std::mutex> lock(_input_mtx);
			for (auto &j : jobs) {
				_inputs.emplace_back(std::move(j));
			}
		}
		_signal.notify_all();
	}

	std::vector<manager::_async_loader::job_result> manager::_async_loader::get_completed_jobs() {
		std::unique_lock<std::mutex> lock(_output_mtx);
		return std::move(_outputs);
	}

	void manager::_async_loader::_job_thread_func() {
		while (_state != state::shutting_down) {
			std::vector<job> jobs;
			{
				std::unique_lock<std::mutex> lock(_input_mtx);
				if (_inputs.empty()) {
					_signal.wait(lock);
				}
				jobs = std::exchange(_inputs, {});
			}

			std::size_t i = 0;
			for (; i < jobs.size(); ++i) {
				auto result = _process_job(std::move(jobs[i]));
				{
					std::unique_lock<std::mutex> lock(_output_mtx);
					_outputs.emplace_back(std::move(result));
				}
				if (_state == state::shutting_down) {
					std::unique_lock<std::mutex> lock(_output_mtx);
					for (++i; i < jobs.size(); ++i) {
						_outputs.emplace_back(std::move(jobs[i]), nullptr);
					}
					return;
				}
			}
		}
	}

	manager::_async_loader::job_result manager::_async_loader::_process_job(job j) {
		// load image binary
		// TODO subpath is ignored
		auto [image_mem, image_size] = load_binary_file(j.path.string().c_str());
		if (!image_mem) {
			log().error("Failed to open image file: {}", j.path.string());
			return job_result(std::move(j), nullptr);
		}

		if (j.path.extension() == ".dds") {
			auto *data = image_mem.get();
			if (auto loaded = dds::loader::create({ data, data + image_size })) {
				std::vector<job_result::subresource> mips;

				std::uint32_t num_mips = loaded->get_num_mips();
				const auto &format_props = gpu::format_properties::get(loaded->get_format());
				auto frag_size = format_props.fragment_size.into<std::uint32_t>();
				cvec2u32 one(1, 1);
				auto raw_data = loaded->get_raw_data();
				auto current = raw_data.begin();
				for (std::uint32_t i = 0; i < num_mips; ++i) {
					cvec2u32 pixel_size = vec::memberwise_max(
						cvec2u32(loaded->get_width() >> i, loaded->get_height() >> i), one
					);
					cvec2u32 num_fragments = vec::memberwise_divide(pixel_size + frag_size - one, frag_size);
					std::size_t size = num_fragments[0] * num_fragments[1] * format_props.bytes_per_fragment;

					if (raw_data.end() - current < static_cast<std::ptrdiff_t>(size)) {
						log().error("{}: Not enough space for mip {} and below", j.path.string(), i);
						break;
					}

					mips.emplace_back(&*current, i);
					current += size;
				}

				// TODO more verifications?
				return job_result(
					std::move(j),
					loader_type::dds,
					cvec2u32(loaded->get_width(), loaded->get_height()),
					loaded->get_format(),
					std::move(mips),
					[blob = std::move(image_mem)]() mutable {
						blob = nullptr;
					}
				);
			} else {
				return job_result(std::move(j), nullptr);
			}
		} else {
			// get image format and load
			void *loaded = nullptr;
			std::uint8_t bytes_per_channel = 1;
			int width = 0;
			int height = 0;
			int original_channels = 0;
			auto type = gpu::format_properties::data_type::unknown;
			auto *stbi_mem = reinterpret_cast<const stbi_uc*>(image_mem.get());
			if (stbi_is_hdr_from_memory(stbi_mem, static_cast<int>(image_size))) {
				type = gpu::format_properties::data_type::floating_point;
				bytes_per_channel = 4;
				loaded = stbi_loadf_from_memory(
					stbi_mem, static_cast<int>(image_size), &width, &height, &original_channels, 4
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
			image_mem.reset(); // we're done loading; free the loaded image file
			std::uint8_t num_channels = original_channels == 3 ? 4 : static_cast<std::uint8_t>(original_channels);
			std::uint8_t bits_per_channel_4[4] = { 0, 0, 0, 0 };
			for (int i = 0; i < num_channels; ++i) {
				bits_per_channel_4[i] = bytes_per_channel * 8;
			}

			auto pixel_format = gpu::format_properties::find_exact_rgba(
				bits_per_channel_4[0], bits_per_channel_4[1], bits_per_channel_4[2], bits_per_channel_4[3], type
			);
			if (pixel_format == gpu::format::none) {
				log().error(
					"Failed to find appropriate pixel format for bytes per channel {}, type {}",
					bytes_per_channel, static_cast<std::underlying_type_t<gpu::format_properties::data_type>>(type)
				);
			}

			return job_result(
				std::move(j),
				loader_type::stbi,
				cvec2i(width, height).into<std::uint32_t>(),
				pixel_format,
				{ job_result::subresource(static_cast<const std::byte*>(loaded), 0) },
				[ptr = loaded]() {
					stbi_image_free(ptr);
				}
			);
		}
	}


	[[nodiscard]] handle<image2d> manager::get_image2d(const identifier &id, const pool &p) {
		if (auto it = _images.find(id); it != _images.end()) {
			if (auto ptr = it->second.lock()) {
				return handle<image2d>(std::move(ptr));
			}
		}

		// create object
		image2d tex = nullptr;
		tex.image = get_invalid_image()->image;
		tex.descriptor_index = _allocate_descriptor_index();
		_context.write_image_descriptors(_image2d_descriptors, tex.descriptor_index, { tex.image });
		auto result = _register_asset(id, std::move(tex), _images);
		_input_jobs.emplace_back(result, p);
		return result;
	}

	handle<buffer> manager::create_buffer(
		identifier id, std::span<const std::byte> data, gpu::buffer_usage_mask usages, const pool &p
	) {
		buffer buf = nullptr;
		buf.data = _context.request_buffer(
			id.path.u8string() + u8"|" + id.subpath,
			static_cast<std::uint32_t>(data.size()),
			usages | gpu::buffer_usage_mask::copy_destination,
			p
		);
		_upload_queue.upload_buffer(buf.data, data, 0, u8"Load buffer asset");
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
			std::move(id), { binary.get(), size }, stage, entry_point, defines
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
		return _do_compile_shader_library_from_source(std::move(id), { binary.get(), size }, defines);
	}

	void manager::update() {
		if (!_input_jobs.empty()) {
			_image_loader.add_jobs(std::move(_input_jobs));
		}
		auto finished_jobs = _image_loader.get_completed_jobs();
		for (auto &j : finished_jobs) {
			if (!j.results.empty()) {
				auto &tex = j.input.target._ptr->value;
				const auto &format_props = gpu::format_properties::get(j.pixel_format);
				auto usages = gpu::image_usage_mask::copy_destination | gpu::image_usage_mask::shader_read;
				if (!format_props.has_compressed_color()) {
					usages |= gpu::image_usage_mask::shader_write;
				}

				// find out how many mips we've loaded
				std::uint32_t highest_mip = j.results[0].mip;
				std::uint32_t lowest_mip = j.results[0].mip;
				std::uint32_t mip_mask = 0;
				for (const auto &res : j.results) {
					highest_mip = std::min(highest_mip, res.mip);
					lowest_mip = std::max(lowest_mip, res.mip);
					mip_mask |= 1 << res.mip;
				}

				tex.image = _context.request_image2d(
					j.input.path.u8string(), j.size, lowest_mip + 1, j.pixel_format, usages, j.input.memory_pool
				);
				tex.highest_mip_loaded = highest_mip;

				// upload image
				for (const auto &res : j.results) {
					auto view = tex.image.view_mips(gpu::mip_levels::only(res.mip));
					_upload_queue.upload_image(view, res.data, u8"Upload image"); // TODO better label
				}

				j.destroy();

				_context.write_image_descriptors(
					_image2d_descriptors, tex.descriptor_index,
					{ tex.image.view_mips(gpu::mip_levels::all_below(tex.highest_mip_loaded)) }
				);
				log().debug("Texture {} loaded", j.input.path.string());
			}
		}
	}

	manager::manager(context &ctx, context::queue q, gpu::shader_utility *shader_utils) :
		_context(ctx), _upload_queue(q), _shader_utilities(shader_utils),
		_image2d_descriptors(ctx.request_image_descriptor_array(
			u8"Texture assets", gpu::descriptor_type::read_only_image, 1024
		)),
		_sampler_descriptors(ctx.create_cached_descriptor_set(u8"Samplers", {
			{ 0, sampler_state(
				gpu::filtering::linear, gpu::filtering::linear, gpu::filtering::linear,
				0.0f, 0.0f, std::numeric_limits<float>::max(), 16.0f
			) },
			{ 1, sampler_state(
				gpu::filtering::linear, gpu::filtering::linear, gpu::filtering::linear,
				0.0f, 0.0f, std::numeric_limits<float>::max(), 16.0f,
				gpu::sampler_address_mode::clamp, gpu::sampler_address_mode::clamp, gpu::sampler_address_mode::clamp
			) },
		})),
		_image2d_descriptor_index_alloc({ 0 }),
		_invalid_image(nullptr),
		_null_image(nullptr),
		_default_normal_image(nullptr) {

		{ // create "invalid" texture
			constexpr cvec2u32 size(128, 128);
			{
				image2d tex = nullptr;
				tex.image = _context.request_image2d(
					u8"Invalid", size, 1, gpu::format::r8g8b8a8_unorm,
					gpu::image_usage_mask::copy_destination | gpu::image_usage_mask::shader_read,
					nullptr // TODO pool?
				);
				tex.descriptor_index = _allocate_descriptor_index();
				tex.highest_mip_loaded = 0;
				_context.write_image_descriptors(_image2d_descriptors, tex.descriptor_index, { tex.image });
				_invalid_image = _register_asset(assets::identifier({}, u8"invalid"), std::move(tex), _images);
			}

			std::vector<linear_rgba_u8> tex_data(size[0] * size[1], zero);
			for (std::uint32_t y = 0; y < size[1]; ++y) {
				for (std::uint32_t x = 0; x < size[0]; ++x) {
					tex_data[y * size[1] + x] =
						(x ^ y) & 1 ? linear_rgba_u8(255, 0, 255, 255) : linear_rgba_u8(0, 255, 0, 255);
				}
			}
			_upload_queue.upload_image(
				_invalid_image->image, reinterpret_cast<std::byte*>(tex_data.data()), u8"Invalid"
			);
		}

		{ // create "null" texture
			{
				image2d tex = nullptr;
				tex.image = _context.request_image2d(
					u8"Null", cvec2u32(1, 1), 1, gpu::format::r8g8b8a8_unorm,
					gpu::image_usage_mask::copy_destination | gpu::image_usage_mask::shader_read,
					nullptr // TODO pool?
				);
				tex.descriptor_index = _allocate_descriptor_index();
				tex.highest_mip_loaded = 0;
				_context.write_image_descriptors(_image2d_descriptors, tex.descriptor_index, { tex.image });
				_null_image = _register_asset<image2d>(assets::identifier({}, u8"null"), std::move(tex), _images);
			}

			linear_rgba_u8 tex_data(0, 0, 0, 0);
			_upload_queue.upload_image(_null_image->image, reinterpret_cast<std::byte*>(&tex_data), u8"Null");
		}

		{ // create "null normal" texture
			{
				image2d tex = nullptr;
				tex.image = _context.request_image2d(
					u8"Default Normal", cvec2u32(1, 1), 1, gpu::format::r8g8b8a8_unorm,
					gpu::image_usage_mask::copy_destination | gpu::image_usage_mask::shader_read,
					nullptr // TODO pool?
				);
				tex.descriptor_index = _allocate_descriptor_index();
				tex.highest_mip_loaded = 0;
				_context.write_image_descriptors(_image2d_descriptors, tex.descriptor_index, { tex.image });
				_default_normal_image = _register_asset<image2d>(
					assets::identifier({}, u8"default_normal"), std::move(tex), _images
				);
			}

			linear_rgba_u8 tex_data(127, 127, 255, 0);
			_upload_queue.upload_image(
				_default_normal_image->image, reinterpret_cast<std::byte*>(&tex_data), u8"Default Normal"
			);
		}
	}

	std::u8string manager::_assemble_shader_subid(
		gpu::shader_stage stage,
		std::u8string_view entry_point,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		static constexpr enums::sequential_mapping<gpu::shader_stage, std::u8string_view> _shader_stage_names{
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

		auto result = _shader_utilities->compile_shader(
			code, stage, entry_point, id.path, additional_shader_include_paths, defines
		);
		auto output = result.get_compiler_output();
		if (!result.succeeded()) {
			log().error("Failed to compile shader {} ({}):", id.path.string(), string::to_generic(id.subpath));
			log().error("{}", string::to_generic(output));
			return nullptr;
		} else {
			if (!output.empty()) {
				log().debug(
					"Shader compiler output for {} ({}):", id.path.string(), string::to_generic(id.subpath)
				);
				log().debug("{}", string::to_generic(output));
			}
		}

		shader res = nullptr;
		auto binary = result.get_compiled_binary();
		res.binary     = context::device_access::get(_context).load_shader(binary);
		res.reflection = _shader_utilities->load_shader_reflection(binary);
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

		auto result = _shader_utilities->compile_shader_library(
			code, id.path, additional_shader_include_paths, defines
		);
		auto output = result.get_compiler_output();
		if (!result.succeeded()) {
			log().error("Failed to compile shader {} ({}):", id.path.string(), string::to_generic(id.subpath));
			log().error("{}", string::to_generic(output));
			return nullptr;
		} else {
			if (!output.empty()) {
				log().debug(
					"Shader compiler output for {} ({}):", id.path.string(), string::to_generic(id.subpath)
				);
				log().debug("{}", string::to_generic(output));
			}
		}

		shader_library res = nullptr;
		auto binary = result.get_compiled_binary();
		res.binary     = context::device_access::get(_context).load_shader(binary);
		res.reflection = _shader_utilities->load_shader_library_reflection(binary);
		return _register_asset(std::move(id), std::move(res), _shader_libraries);
	}
}
