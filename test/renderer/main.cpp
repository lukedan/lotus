#include <iostream>

#include <lotus/math/vector.h>
#include <lotus/graphics/context.h>
#include <lotus/graphics/commands.h>
#include <lotus/graphics/descriptors.h>
#include <lotus/system/window.h>
#include <lotus/system/application.h>

int main() {
	lotus::system::application app(u8"test");
	lotus::graphics::context ctx;

	lotus::graphics::device dev = nullptr;
	ctx.enumerate_adapters([&](lotus::graphics::adapter adap) {
		dev = adap.create_device();
		return false;
	});
	auto cmd_queue = dev.create_command_queue();
	auto cmd_alloc = dev.create_command_allocator();

	constexpr std::size_t num_swapchain_images = 2;

	auto wnd = app.create_window();
	auto swapchain = ctx.create_swap_chain_for_window(
		wnd, dev, cmd_queue, num_swapchain_images, lotus::graphics::pixel_format::r8g8b8a8_unorm
	);

	std::array<lotus::graphics::render_target_pass_options, 1> rtv_options{
		lotus::graphics::render_target_pass_options::create(
			lotus::graphics::pixel_format::r8g8b8a8_unorm,
			lotus::graphics::pass_load_operation::clear, lotus::graphics::pass_store_operation::preserve
		)
	};
	auto dsv_options = lotus::graphics::depth_stencil_pass_options::create(
		lotus::graphics::pixel_format::none,
		lotus::graphics::pass_load_operation::discard, lotus::graphics::pass_store_operation::discard,
		lotus::graphics::pass_load_operation::discard, lotus::graphics::pass_store_operation::discard
	);
	auto pass_resources = dev.create_pass_resources(rtv_options, dsv_options);

	std::array<lotus::graphics::image2d_view, num_swapchain_images> views{ nullptr, nullptr };
	std::array<lotus::graphics::frame_buffer, num_swapchain_images> frame_buffers{ nullptr, nullptr };
	for (std::size_t i = 0; i < num_swapchain_images; ++i) {
		views[i] = dev.create_image2d_view_from(
			swapchain.get_image(i),
			lotus::graphics::pixel_format::r8g8b8a8_unorm,
			lotus::graphics::mip_levels::only_highest()
		);
		std::array<const lotus::graphics::image2d_view*, 1> rtv_views{ &views[i] };
		frame_buffers[i] = dev.create_frame_buffer(rtv_views, nullptr, pass_resources);
	}

	// create & fill vertex buffer
	struct _vertex {
		lotus::cvec3f position = lotus::uninitialized;
	};
	std::size_t num_verts = 3;
	std::size_t buffer_size = sizeof(_vertex) * num_verts;
	auto vertex_buffer = dev.create_committed_buffer(
		buffer_size, lotus::graphics::heap_type::device_only, lotus::graphics::buffer_usage::copy_destination
	);
	{
		auto upload_buffer = dev.create_committed_buffer(
			buffer_size, lotus::graphics::heap_type::upload, lotus::graphics::buffer_usage::copy_source
		);
		void *raw_ptr = dev.map_buffer(upload_buffer, 0, 0);
		auto *ptr = new (raw_ptr) _vertex[num_verts];
		ptr[0].position = lotus::cvec3f(-0.5f, -0.5f, -0.5f);
		ptr[1].position = lotus::cvec3f(0.5f, 0.0f, -0.5f);
		ptr[2].position = lotus::cvec3f(-0.5f, 0.5f, -0.5f);
		dev.unmap_buffer(upload_buffer, 0, buffer_size);

		auto copy_cmd = dev.create_command_list(cmd_alloc);
		copy_cmd.start();
		copy_cmd.copy_buffer(upload_buffer, 0, vertex_buffer, 0, buffer_size);
		std::array<lotus::graphics::buffer_barrier, 1> image_barrier{
			lotus::graphics::buffer_barrier::create(
				vertex_buffer,
				lotus::graphics::buffer_usage::copy_destination,
				lotus::graphics::buffer_usage::vertex_buffer
			)
		};
		copy_cmd.resource_barrier({}, image_barrier);
		copy_cmd.finish();

		std::array<const lotus::graphics::command_list*, 1> lists{ &copy_cmd };
		auto fence = dev.create_fence(lotus::graphics::synchronization_state::unset);
		cmd_queue.submit_command_lists(lists, &fence);
		dev.wait_for_fence(fence);
	}

	std::array<lotus::graphics::command_list, num_swapchain_images> lists{ nullptr, nullptr };
	std::array<lotus::graphics::fence, num_swapchain_images> fences{ nullptr, nullptr };
	for (std::size_t i = 0; i < num_swapchain_images; ++i) {
		char8_t buffer[20];
		std::snprintf(reinterpret_cast<char*>(buffer), std::size(buffer), "Back buffer %d", static_cast<int>(i));
		lotus::graphics::image2d img = swapchain.get_image(i);
		dev.set_debug_name(img, buffer);

		fences[i] = dev.create_fence(lotus::graphics::synchronization_state::set);

		lists[i] = dev.create_command_list(cmd_alloc);
		lists[i].start();
		{
			std::array<lotus::graphics::image_barrier, 1> image_barrier{
				lotus::graphics::image_barrier::create(
					img, lotus::graphics::image_usage::present, lotus::graphics::image_usage::color_render_target
				)
			};
			lists[i].resource_barrier(image_barrier, {});
		}
		std::array<lotus::linear_rgba_f, 1> clear_color{ lotus::linear_rgba_f(1.0f, 0.0f, 0.0f, 1.0f) };
		lists[i].begin_pass(pass_resources, frame_buffers[i], clear_color, 0.0f, 0);
		std::array<lotus::graphics::vertex_buffer, 1> vertex_buffers{
			lotus::graphics::vertex_buffer::from_buffer_stride(vertex_buffer, sizeof(_vertex))
		};
		lists[i].bind_vertex_buffers(0, vertex_buffers);
		// TODO draw something
		lists[i].end_pass();
		{
			std::array<lotus::graphics::image_barrier, 1> image_barrier{
				lotus::graphics::image_barrier::create(
					img, lotus::graphics::image_usage::color_render_target, lotus::graphics::image_usage::present
				)
			};
			lists[i].resource_barrier(image_barrier, {});
		}
		lists[i].finish();
	}

	wnd.show_and_activate();
	while (app.process_message_nonblocking() != lotus::system::message_type::quit) {
		auto back_buffer = swapchain.acquire_back_buffer();

		if (back_buffer.on_presented) {
			dev.wait_for_fence(*back_buffer.on_presented);
			dev.reset_fence(*back_buffer.on_presented);
		}
		lotus::graphics::image2d img = swapchain.get_image(back_buffer.index);

		std::array<const lotus::graphics::command_list*, 1> submit_lists{ &lists[back_buffer.index] };
		cmd_queue.submit_command_lists(submit_lists, nullptr);

		cmd_queue.present(swapchain, &fences[back_buffer.index]);
	}

	return 0;
}
