#include <iostream>
#include <fstream>
#include <filesystem>

#include <lotus/math/vector.h>
#include <lotus/graphics/context.h>
#include <lotus/graphics/commands.h>
#include <lotus/graphics/descriptors.h>
#include <lotus/system/window.h>
#include <lotus/system/application.h>

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

	std::array<gfx::render_target_pass_options, 1> rtv_options{
		gfx::render_target_pass_options::create(
			gfx::format::r8g8b8a8_unorm,
			gfx::pass_load_operation::clear, gfx::pass_store_operation::preserve
		)
	};
	auto dsv_options = gfx::depth_stencil_pass_options::create(
		gfx::format::none,
		gfx::pass_load_operation::discard, gfx::pass_store_operation::discard,
		gfx::pass_load_operation::discard, gfx::pass_store_operation::discard
	);
	auto pass_resources = dev.create_pass_resources(rtv_options, dsv_options);

	std::array<gfx::image2d_view, num_swapchain_images> views{ nullptr, nullptr };
	std::array<gfx::frame_buffer, num_swapchain_images> frame_buffers{ nullptr, nullptr };
	for (std::size_t i = 0; i < num_swapchain_images; ++i) {
		views[i] = dev.create_image2d_view_from(
			swapchain.get_image(i),
			gfx::format::r8g8b8a8_unorm,
			gfx::mip_levels::only_highest()
		);
		std::array<const gfx::image2d_view*, 1> rtv_views{ &views[i] };
		frame_buffers[i] = dev.create_frame_buffer(rtv_views, nullptr, pass_resources);
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
		buffer_size, gfx::heap_type::device_only, gfx::buffer_usage::copy_destination
	);
	{
		auto upload_buffer = dev.create_committed_buffer(
			buffer_size, gfx::heap_type::upload, gfx::buffer_usage::copy_source
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
		std::array<gfx::buffer_barrier, 1> image_barrier{
			gfx::buffer_barrier::create(
				vertex_buffer,
				gfx::buffer_usage::copy_destination,
				gfx::buffer_usage::vertex_buffer
			)
		};
		copy_cmd.resource_barrier({}, image_barrier);
		copy_cmd.finish();

		std::array<const gfx::command_list*, 1> lists{ &copy_cmd };
		auto fence = dev.create_fence(gfx::synchronization_state::unset);
		cmd_queue.submit_command_lists(lists, &fence);
		dev.wait_for_fence(fence);
	}

	auto pipeline_rsrc = dev.create_pipeline_resources({});
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
	std::array<gfx::input_buffer_layout, 1> input_buffer{
		gfx::input_buffer_layout::create_vertex_buffer<_vertex>(vert_buffer_elements, 0),
	};
	auto pipeline = dev.create_pipeline_state(pipeline_rsrc, shaders, blend, rasterizer, depth_stencil, input_buffer, gfx::primitive_topology::triangle_list, pass_resources);

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

		{
			std::array<gfx::image_barrier, 1> image_barrier{
				gfx::image_barrier::create(img, gfx::image_usage::present, gfx::image_usage::color_render_target)
			};
			lists[i].resource_barrier(image_barrier, {});
		}

		std::array<linear_rgba_f, 1> clear_color{ linear_rgba_f(1.0f, 0.0f, 0.0f, 1.0f) };
		lists[i].begin_pass(pass_resources, frame_buffers[i], clear_color, 0.0f, 0);

		std::array<gfx::vertex_buffer, 1> vertex_buffers{
			gfx::vertex_buffer::from_buffer_stride(vertex_buffer, sizeof(_vertex))
		};
		lists[i].bind_pipeline_state(pipeline);
		lists[i].bind_vertex_buffers(0, vertex_buffers);
		std::array<gfx::viewport, 1> viewports{
			gfx::viewport::create(aab2f::create_from_min_max(zero, cvec2f(800.0f, 600.0f)), 0.0f, 1.0f)
		};
		lists[i].set_viewports(viewports);

		lists[i].draw_instanced(0, num_verts, 0, 1);

		lists[i].end_pass();

		{
			std::array<gfx::image_barrier, 1> image_barrier{
				gfx::image_barrier::create(img, gfx::image_usage::color_render_target, gfx::image_usage::present)
			};
			lists[i].resource_barrier(image_barrier, {});
		}

		lists[i].finish();
	}

	wnd.show_and_activate();
	while (app.process_message_nonblocking() != sys::message_type::quit) {
		auto back_buffer = swapchain.acquire_back_buffer();

		if (back_buffer.on_presented) {
			dev.wait_for_fence(*back_buffer.on_presented);
			dev.reset_fence(*back_buffer.on_presented);
		}
		gfx::image2d img = swapchain.get_image(back_buffer.index);

		std::array<const gfx::command_list*, 1> submit_lists{ &lists[back_buffer.index] };
		cmd_queue.submit_command_lists(submit_lists, nullptr);

		cmd_queue.present(swapchain, &fences[back_buffer.index]);
	}

	return 0;
}
