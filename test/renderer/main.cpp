#include <iostream>
#include <fstream>
#include <filesystem>

#include <tiny_gltf.h>

#include <lotus/math/vector.h>
#include <lotus/graphics/context.h>
#include <lotus/graphics/commands.h>
#include <lotus/graphics/descriptors.h>
#include <lotus/system/window.h>
#include <lotus/system/application.h>

namespace gltf = tinygltf;

using namespace lotus;
namespace sys = lotus::system;
namespace gfx = lotus::graphics;

std::vector<std::byte> load_binary_file(const std::filesystem::path &p) {
	std::ifstream fin(p, std::ios::binary | std::ios::ate);
	assert(fin.good());
	std::vector<std::byte> result(fin.tellg());
	fin.seekg(0, std::ios::beg);
	fin.read(reinterpret_cast<char*>(result.data()), result.size());
	return result;
}

int main() {
	sys::application app(u8"test");
	gfx::context ctx;

	gfx::device dev = nullptr;
	ctx.enumerate_adapters([&](gfx::adapter adap) {
		auto properties = adap.get_properties();
		dev = adap.create_device();
		return false;
	});
	auto cmd_queue = dev.create_command_queue();
	auto cmd_alloc = dev.create_command_allocator();

	constexpr std::size_t num_swapchain_images = 2;

	auto wnd = app.create_window();
	auto swapchain = ctx.create_swap_chain_for_window(
		wnd, dev, cmd_queue, num_swapchain_images, gfx::format::r8g8b8a8_unorm
	);

	auto pass_resources = dev.create_pass_resources(
		{
			gfx::render_target_pass_options::create(
				gfx::format::r8g8b8a8_unorm,
				gfx::pass_load_operation::clear, gfx::pass_store_operation::preserve
			)
		},
		gfx::depth_stencil_pass_options::create(
			gfx::format::none,
			gfx::pass_load_operation::discard, gfx::pass_store_operation::discard,
			gfx::pass_load_operation::discard, gfx::pass_store_operation::discard
		)
	);

	std::array<gfx::image2d_view, num_swapchain_images> views{ nullptr, nullptr };
	std::array<gfx::frame_buffer, num_swapchain_images> frame_buffers{ nullptr, nullptr };
	for (std::size_t i = 0; i < num_swapchain_images; ++i) {
		views[i] = dev.create_image2d_view_from(
			swapchain.get_image(i),
			gfx::format::r8g8b8a8_unorm,
			gfx::mip_levels::only_highest()
		);
		frame_buffers[i] = dev.create_frame_buffer({ &views[i] }, nullptr, pass_resources);
	}


	gltf::Model model;
	{
		gltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		std::string path = "../../thirdparty/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf";
		if (!loader.LoadASCIIFromFile(&model, &err, &warn, path)) {
			std::cout << "Failed to load scene: " << err << "\n";
		}
		std::cout << warn << "\n";
	}


	std::vector<gfx::image2d> images;
	std::vector<gfx::buffer> buffers;
	{ // upload resources
		// images
		images.reserve(model.images.size());
		auto upload_fence = dev.create_fence(gfx::synchronization_state::unset);
		for (const auto &img : model.images) {
			std::size_t bytes_per_pixel = (img.component * img.bits + 7) / 8;
			std::size_t bytes_per_row   = bytes_per_pixel * img.width;
			// TODO find correct pixel format
			auto format = gfx::format::r8g8b8a8_unorm;
			auto [upload_buf, upload_buf_layout] = dev.create_committed_buffer_as_image2d(
				img.width, img.height, format, gfx::heap_type::upload,
				gfx::buffer_usage::mask::copy_source, gfx::buffer_usage::copy_source
			);
			// copy data
			auto *dest = static_cast<std::byte*>(dev.map_buffer(upload_buf, 0, 0));
			auto *source = img.image.data();
			for (
				std::size_t y = 0;
				y < img.height;
				++y, dest += upload_buf_layout.row_pitch, source += bytes_per_row
			) {
				std::memcpy(dest, source, bytes_per_row);
			}
			dev.unmap_buffer(upload_buf, 0, upload_buf_layout.total_size);

			// create gpu image
			auto gpu_img = dev.create_committed_image2d(
				img.width, img.height, 1, 1, format, gfx::image_tiling::optimal,
				gfx::image_usage::mask::copy_destination | gfx::image_usage::mask::read_only_texture,
				gfx::image_usage::copy_destination
			);

			// copy to gpu
			dev.reset_fence(upload_fence);
			auto copy_cmd = dev.create_command_list(cmd_alloc);
			copy_cmd.start();
			copy_cmd.copy_buffer_to_image(
				upload_buf, 0, upload_buf_layout.row_pitch,
				aab2s::create_from_min_max(zero, cvec2s(img.width, img.height)), gpu_img, 0, zero
			);
			copy_cmd.resource_barrier(
				{
					gfx::image_barrier::create(
						gpu_img, gfx::image_usage::copy_destination, gfx::image_usage::read_only_texture
					)
				},
				{}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);

			dev.set_debug_name(gpu_img, reinterpret_cast<const char8_t*>(img.name.c_str()));

			images.emplace_back(std::move(gpu_img));
		}

		// buffers
		// TODO
	}


	auto mat_descriptor_set_layout = dev.create_descriptor_set_layout(
		{
			gfx::descriptor_range_binding::create(gfx::descriptor_range::create(gfx::descriptor_type::read_only_image, 1), 0),
			gfx::descriptor_range_binding::create(gfx::descriptor_range::create(gfx::descriptor_type::sampler, 1), 1),
		}, gfx::shader_stage_mask::pixel_shader
	);
	auto descriptor_pool = dev.create_descriptor_pool(
		{
			gfx::descriptor_range::create(gfx::descriptor_type::read_only_image, 100),
			gfx::descriptor_range::create(gfx::descriptor_type::sampler, 100),
		}, 100
	);


	auto sampler = dev.create_sampler(
		gfx::filtering::nearest, gfx::filtering::nearest, gfx::filtering::nearest, 0, 0, 1, std::nullopt,
		gfx::sampler_address_mode::repeat, gfx::sampler_address_mode::repeat, gfx::sampler_address_mode::repeat,
		linear_rgba_f(1.f, 1.f, 1.f, 1.f), std::nullopt
	);
	std::vector<gfx::descriptor_set> descriptor_sets;
	{
		for (const auto &mat : model.materials) {
			auto &base_color_tex = images[mat.pbrMetallicRoughness.baseColorTexture.index];
			auto view = dev.create_image2d_view_from(base_color_tex, gfx::format::r8g8b8a8_unorm, gfx::mip_levels::only_highest());
			auto set = dev.create_descriptor_set(descriptor_pool, mat_descriptor_set_layout);
			dev.write_descriptor_set_images(set, mat_descriptor_set_layout, 0, { &view });
			dev.write_descriptor_set_samplers(set, mat_descriptor_set_layout, 1, { &sampler });
			descriptor_sets.emplace_back(std::move(set));
		}
	}


	// create & fill vertex buffer
	struct _vertex {
		cvec4f position = uninitialized;
		cvec2f uv = uninitialized;
		cvec2f _padding = uninitialized;
	};
	constexpr std::size_t num_verts = 3;
	constexpr std::size_t buffer_size = sizeof(_vertex) * num_verts;
	auto vertex_buffer = dev.create_committed_buffer(
		buffer_size, gfx::heap_type::device_only,
		gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::vertex_buffer,
		gfx::buffer_usage::copy_destination
	);
	{
		auto upload_buffer = dev.create_committed_buffer(
			buffer_size, gfx::heap_type::upload,
			gfx::buffer_usage::mask::copy_source, gfx::buffer_usage::copy_source
		);
		void *raw_ptr = dev.map_buffer(upload_buffer, 0, 0);
		auto *ptr = new (raw_ptr) _vertex[num_verts];
		ptr[0].position = cvec4f(-0.5f, -0.5f, 0.5f, 1.0f);
		ptr[1].position = cvec4f( 0.5f,  0.0f, 0.5f, 1.0f);
		ptr[2].position = cvec4f(-0.5f,  0.5f, 0.5f, 1.0f);
		ptr[0].uv = cvec2f(0.0f, 0.0f);
		ptr[1].uv = cvec2f(1.0f, 0.0f);
		ptr[2].uv = cvec2f(0.0f, 1.0f);
		dev.unmap_buffer(upload_buffer, 0, buffer_size);

		auto copy_cmd = dev.create_command_list(cmd_alloc);
		copy_cmd.start();
		copy_cmd.copy_buffer(upload_buffer, 0, vertex_buffer, 0, buffer_size);
		copy_cmd.resource_barrier(
			{},
			{
				gfx::buffer_barrier::create(
					vertex_buffer,
					gfx::buffer_usage::copy_destination,
					gfx::buffer_usage::vertex_buffer
				)
			}
		);
		copy_cmd.finish();

		auto fence = dev.create_fence(gfx::synchronization_state::unset);
		cmd_queue.submit_command_lists({ &copy_cmd }, &fence);
		dev.wait_for_fence(fence);
	}

	auto pipeline_rsrc = dev.create_pipeline_resources({ &mat_descriptor_set_layout });
	std::vector<std::byte> vert_shader_code = load_binary_file("test.vs.o"); // TODO
	std::vector<std::byte> pix_shader_code = load_binary_file("test.ps.o"); // TODO
	auto vert_shader = dev.load_shader(vert_shader_code);
	auto pix_shader = dev.load_shader(pix_shader_code);
	auto shaders = gfx::shader_set::create(vert_shader, pix_shader);
	auto blend = gfx::blend_options::create_for_render_targets(
		gfx::render_target_blend_options::disabled()
	);
	auto rasterizer = gfx::rasterizer_options::create(zero, gfx::front_facing_mode::clockwise, gfx::cull_mode::none);
	auto depth_stencil = gfx::depth_stencil_options::all_disabled();
	std::array<gfx::input_buffer_element, 2> vert_buffer_elements{
		gfx::input_buffer_element::create("POSITION", 0, gfx::format::r32g32b32a32_float, offsetof(_vertex, position)),
		gfx::input_buffer_element::create("TEXCOORD", 0, gfx::format::r32g32_float, offsetof(_vertex, uv)),
	};
	auto pipeline = dev.create_pipeline_state(
		pipeline_rsrc, shaders, blend, rasterizer, depth_stencil,
		{
			gfx::input_buffer_layout::create_vertex_buffer<_vertex>(vert_buffer_elements, 0),
		}, gfx::primitive_topology::triangle_list, pass_resources
	);

	std::array<gfx::command_list, num_swapchain_images> lists{ nullptr, nullptr };
	std::array<gfx::fence, num_swapchain_images> fences{ nullptr, nullptr };
	for (std::size_t i = 0; i < num_swapchain_images; ++i) {
		char8_t buffer[20];
		std::snprintf(reinterpret_cast<char*>(buffer), std::size(buffer), "Back buffer %d", static_cast<int>(i));
		gfx::image2d img = swapchain.get_image(i);
		dev.set_debug_name(img, buffer);

		fences[i] = dev.create_fence(gfx::synchronization_state::set);

		lists[i] = dev.create_command_list(cmd_alloc);
		lists[i].start();

		lists[i].resource_barrier(
			{
				gfx::image_barrier::create(img, gfx::image_usage::present, gfx::image_usage::color_render_target)
			}, {}
		);

		lists[i].begin_pass(pass_resources, frame_buffers[i], { linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f) }, 0.0f, 0);

		lists[i].bind_pipeline_state(pipeline);
		lists[i].bind_vertex_buffers(0, { gfx::vertex_buffer::from_buffer_stride(vertex_buffer, sizeof(_vertex)) });
		lists[i].bind_descriptor_sets(0, { &descriptor_sets[0] });

		lists[i].set_viewports({ gfx::viewport::create(aab2f::create_from_min_max(zero, cvec2f(800.0f, 600.0f)), 0.0f, 1.0f) });
		lists[i].set_scissor_rectangles({ aab2i::create_from_min_max(zero, cvec2i(800, 600)) });

		lists[i].draw_instanced(0, num_verts, 0, 1);

		lists[i].end_pass();

		lists[i].resource_barrier(
			{
				gfx::image_barrier::create(img, gfx::image_usage::color_render_target, gfx::image_usage::present)
			}, {}
		);

		lists[i].finish();
	}

	wnd.show_and_activate();
	while (app.process_message_nonblocking() != sys::message_type::quit) {
		auto back_buffer = swapchain.acquire_back_buffer();

		if (back_buffer.on_presented) {
			dev.wait_for_fence(*back_buffer.on_presented);
			dev.reset_fence(*back_buffer.on_presented);
		}

		cmd_queue.submit_command_lists({ &lists[back_buffer.index] }, nullptr);

		cmd_queue.present(swapchain, &fences[back_buffer.index]);
	}

	return 0;
}
