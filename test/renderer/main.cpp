#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

#include <tiny_gltf.h>

#include <lotus/math/vector.h>
#include <lotus/graphics/context.h>
#include <lotus/graphics/commands.h>
#include <lotus/graphics/descriptors.h>
#include <lotus/system/window.h>
#include <lotus/system/application.h>
#include <lotus/utils/camera.h>
#include <lotus/utils/misc.h>
#include <lotus/renderer/asset_manager.h>
#include <lotus/renderer/gltf_loader.h>
#include <lotus/renderer/g_buffer.h>

#include "common.h"
/*#include "scene.h"
#include "gbuffer_pass.h"
#include "composite_pass.h"
#include "raytrace_pass.h"
#include "rt_resolve_pass.h"*/

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "No model file specified\n";
		return 1;
	}

	std::cout << "Backend: " << std::string_view(reinterpret_cast<const char*>(gfx::backend_name.data()), gfx::backend_name.size()) << "\n";
	std::cout << "Working dir: " << std::filesystem::current_path() << "\n";

	sys::application app(u8"test");
	auto wnd = app.create_window();

	auto shader_util = gfx::shader_utility::create();
	auto ctx = gfx::context::create();
	gfx::device dev = nullptr;
	gfx::adapter_properties dev_prop = uninitialized;
	ctx.enumerate_adapters([&](gfx::adapter adap) {
		dev_prop = adap.get_properties();
		std::cerr << "Device name: " << reinterpret_cast<const char*>(dev_prop.name.c_str()) << "\n";
		if (dev_prop.is_discrete) {
			std::cerr << "  Selected\n";
			dev = adap.create_device();
			return false;
		}
		return true;
	});
	auto cmd_queue = dev.create_command_queue();
	auto cmd_alloc = dev.create_command_allocator();

	auto asset_man = ren::assets::manager::create(dev, cmd_queue, &shader_util);
	auto ren_ctx = ren::context::create(asset_man, ctx, cmd_queue);

	auto g_buffer = ren::g_buffer::view::create(ren_ctx, cvec2s(0, 0));

	// model & resources
	auto gltf_ctx = ren::gltf::context::create(asset_man);
	auto instances = gltf_ctx.load(argv[1]);

	auto sampler = dev.create_sampler(
		gfx::filtering::linear, gfx::filtering::linear, gfx::filtering::linear, 0, 0, 1, 16.0f,
		gfx::sampler_address_mode::repeat, gfx::sampler_address_mode::repeat, gfx::sampler_address_mode::repeat,
		linear_rgba_f(1.f, 1.f, 1.f, 1.f), std::nullopt
	);

	/*composite_pass comp_pass(dev);*/

	/*raytrace_pass rt_pass(dev, model_resources, dev_prop);
	raytrace_resolve_pass rt_resolve_pass(dev);*/

	/*composite_pass::input_resources comp_input;
	std::vector<composite_pass::output_resources> comp_output;*/

	// raytracing buffers
	/*gfx::image2d raytrace_buffer = nullptr;
	gfx::image2d_view raytrace_buffer_view = nullptr;
	raytrace_pass::input_resources rt_input;

	raytrace_resolve_pass::input_resources rt_resolve_input;
	std::vector<raytrace_resolve_pass::output_resources> rt_resolve_output;*/


	gfx::swap_chain swapchain = nullptr;
	std::vector<gfx::fence> present_fences;
	std::vector<gfx::command_list> cmd_lists;

	std::size_t frame_index = 0;

	auto recreate_buffers = [&](cvec2s size) {
		auto beg = std::chrono::high_resolution_clock::now();

		frame_index = 0;

		// reset all resources
		g_buffer = nullptr;
		/*raytrace_buffer = nullptr;
		raytrace_buffer_view = nullptr;*/
		swapchain = nullptr;
		/*comp_output.clear();
		rt_resolve_output.clear();*/
		present_fences.clear();

		// create all surfaces
		auto [sc, fmt] = ctx.create_swap_chain_for_window(
			wnd, dev, cmd_queue, 2, { gfx::format::r8g8b8a8_srgb, gfx::format::b8g8r8a8_srgb }
		);
		swapchain = std::move(sc);
		auto time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - beg).count();

		// create input/output resources
		auto list = dev.create_and_start_command_list(cmd_alloc);

		/*comp_input = comp_pass.create_input_resources(dev, descriptor_pool, gbuf_view);

		raytrace_buffer = dev.create_committed_image2d(size[0], size[1], 1, 1, gfx::format::r32g32b32a32_float, gfx::image_tiling::optimal, gfx::image_usage::mask::read_write_color_texture | gfx::image_usage::mask::read_only_texture);
		raytrace_buffer_view = dev.create_image2d_view_from(raytrace_buffer, gfx::format::r32g32b32a32_float, gfx::mip_levels::only_highest());
		rt_input = rt_pass.create_input_resources(dev, descriptor_pool, model, model_resources, wnd.get_size(), raytrace_buffer_view, sampler);
		list.resource_barrier(
			{
				gfx::image_barrier::create(gfx::subresource_index::first_color(), raytrace_buffer, gfx::image_usage::initial, gfx::image_usage::read_only_texture),
			},
			{}
		);

		rt_resolve_input = rt_resolve_pass.create_input_resources(dev, descriptor_pool, raytrace_buffer_view);*/

		std::size_t num_images = swapchain.get_image_count();
		cmd_lists.clear();
		for (std::size_t i = 0; i < num_images; ++i) {
			auto image = swapchain.get_image(i);
			char8_t buffer[20];
			std::snprintf(reinterpret_cast<char*>(buffer), std::size(buffer), "Back buffer %d", static_cast<int>(i));
			dev.set_debug_name(image, buffer);
			/*comp_output.emplace_back(comp_pass.create_output_resources(dev, image, fmt, size));
			rt_resolve_output.emplace_back(rt_resolve_pass.create_output_resources(dev, image, fmt, size));*/
			present_fences.emplace_back(dev.create_fence(gfx::synchronization_state::unset));
			cmd_lists.emplace_back(nullptr);
			list.resource_barrier(
				{
					gfx::image_barrier(gfx::subresource_index::first_color(), image, gfx::image_usage::initial, gfx::image_usage::present),
				},
				{}
			);
		}

		list.finish();
		auto fence = dev.create_fence(gfx::synchronization_state::unset);
		cmd_queue.submit_command_lists({ &list }, &fence);
		dev.wait_for_fence(fence);

		swapchain.update_synchronization_primitives(present_fences);

		std::cout << "Recreate buffers: " << time << " secs\n";
	};


	auto cam_params = camera_parameters<float>::create_look_at(cvec3f(0.0f, 100.0f, 0.0f), cvec3f(500.0f, 100.0f, 0.0f));
	{
		auto size = wnd.get_size();
		cam_params.far_plane = 10000.0f;
		cam_params.aspect_ratio = size[0] / static_cast<float>(size[1]);
	}
	auto cam = cam_params.into_camera();

	bool resized = true;

	struct {
		gfx::device &dev;
		camera_parameters<float> &cam_params;
		bool &resized;
	} closure = { dev, cam_params, resized };
	auto size_node = wnd.on_resize.create_linked_node(
		[&closure](sys::window&, sys::window_events::resize &info) {
			closure.resized = true;
			closure.cam_params.aspect_ratio = info.new_size[0] / static_cast<float>(info.new_size[1]);
		}
	);
	struct {
		camera_parameters<float> &cam_params;
		camera<float> &cam;
		std::size_t &frame_index;
		bool rotate = false;
		bool zoom = false;
		bool move = false;
		cvec2i prev_mouse = uninitialized;
	} cam_closure = { cam_params, cam, frame_index };
	auto mouse_move_node = wnd.on_mouse_move.create_linked_node(
		[&cam_closure](sys::window&, sys::window_events::mouse::move &move) {
			cvec2f offset = (move.new_position - cam_closure.prev_mouse).into<float>();
			offset[0] = -offset[0];
			if (cam_closure.rotate) {
				cam_closure.cam_params.rotate_around_world_up(offset * 0.004f);
				cam_closure.frame_index = 0;
			}
			if (cam_closure.zoom) {
				cvec3f cam_offset = cam_closure.cam_params.position - cam_closure.cam_params.look_at;
				cam_offset *= std::exp(-0.005f * offset[1]);
				cam_closure.cam_params.position = cam_closure.cam_params.look_at + cam_offset;
				cam_closure.frame_index = 0;
			}
			if (cam_closure.move) {
				cvec3f x = cam_closure.cam.unit_right * offset[0];
				cvec3f y = cam_closure.cam.unit_up * offset[1];
				float distance = (cam_closure.cam_params.position - cam_closure.cam_params.look_at).norm() * 0.001f;
				cvec3f cam_off = (x + y) * distance;
				cam_closure.cam_params.position += cam_off;
				cam_closure.cam_params.look_at += cam_off;
				cam_closure.frame_index = 0;
			}
			cam_closure.prev_mouse = move.new_position;
		}
	);
	auto mouse_down_node = wnd.on_mouse_button_down.create_linked_node(
		[&cam_closure](sys::window &wnd, sys::window_events::mouse::button_down &down) {
			switch (down.button) {
			case sys::mouse_button::primary:
				cam_closure.rotate = true;
				break;
			case sys::mouse_button::secondary:
				cam_closure.zoom = true;
				break;
			case sys::mouse_button::middle:
				cam_closure.move = true;
				break;
			}
			wnd.acquire_mouse_capture();
		}
	);
	auto mouse_up_node = wnd.on_mouse_button_up.create_linked_node(
		[&cam_closure](sys::window &wnd, sys::window_events::mouse::button_up &down) {
			switch (down.button) {
			case sys::mouse_button::primary:
				cam_closure.rotate = false;
				break;
			case sys::mouse_button::secondary:
				cam_closure.zoom = false;
				break;
			case sys::mouse_button::middle:
				cam_closure.move = false;
				break;
			}
			if (!cam_closure.rotate && !cam_closure.zoom && !cam_closure.move) {
				wnd.release_mouse_capture();
			}
		}
	);
	auto capture_broken_node = wnd.on_capture_broken.create_linked_node(
		[&cam_closure](sys::window&) {
			cam_closure.rotate = cam_closure.zoom = cam_closure.move = false;
		}
	);
	auto quit_node = wnd.on_close_request.create_linked_node(
		[&app](sys::window&, sys::window_events::close_request &req) {
			req.should_close = true;
			app.quit();
		}
	);


	wnd.show_and_activate();
	while (app.process_message_nonblocking() != sys::message_type::quit) {
		if (resized) {
			cvec2s size = wnd.get_size();
			if (size[0] == 0 || size[1] == 0) {
				continue;
			}
			recreate_buffers(size);
			resized = false;
		}

		auto back_buffer = dev.acquire_back_buffer(swapchain);
		if (back_buffer.status != gfx::swap_chain_status::ok) {
			resized = true;
			continue;
		}

		if (back_buffer.on_presented) {
			dev.wait_for_fence(*back_buffer.on_presented);
			dev.reset_fence(*back_buffer.on_presented);
		}

		cam = cam_params.into_camera();

		/*{
			auto *data = static_cast<gbuffer_pass::constants*>(dev.map_buffer(gbuf_input.constant_buffer, 0, 0));
			data->view = cam.view_matrix.into<float>();
			data->projection_view = cam.projection_view_matrix.into<float>();
			dev.unmap_buffer(gbuf_input.constant_buffer, 0, sizeof(gbuffer_pass::constants));
		}

		{
			auto *data = static_cast<raytrace_pass::global_data*>(dev.map_buffer(rt_input.constant_buffer, 0, 0));
			data->camera_position = cam_params.position;
			data->t_min = 0.001f;
			float tan_half_y = std::tan(0.5f * cam_params.fov_y_radians);
			auto x = cam.unit_right * cam_params.aspect_ratio * tan_half_y;
			auto y = cam.unit_up * tan_half_y;
			data->top_left = cam.unit_forward - x + y;
			data->t_max = 10000.0f;
			data->right = matf::concat_rows(x * (2.0f / rt_input.output_size[0]), column_vector<1, float>(0.0f));
			data->down = y * (-2.0f / rt_input.output_size[1]);
			data->frame_index = frame_index;
			dev.unmap_buffer(rt_input.constant_buffer, 0, sizeof(raytrace_pass::global_data));
		}

		{
			auto *data = static_cast<raytrace_resolve_pass::global_data*>(dev.map_buffer(rt_resolve_input.globals_buffer, 0, 0));
			data->frame_index = frame_index;
			dev.unmap_buffer(rt_resolve_input.globals_buffer, 0, sizeof(raytrace_resolve_pass::global_data));
		}*/

		auto &cmd_list = cmd_lists[back_buffer.index];
		cmd_list = dev.create_and_start_command_list(cmd_alloc);
		{
			auto image = swapchain.get_image(back_buffer.index);

			/*if constexpr (0) {
				gbuf_pass.record_commands(cmd_list, gbuf, model, model_resources, gbuf_input, gbuf_output);
				comp_pass.record_commands(cmd_list, image, comp_input, comp_output[back_buffer.index]);
			} else {
				rt_pass.record_commands(cmd_list, model, model_resources, rt_input, raytrace_buffer);
				rt_resolve_pass.record_commands(cmd_list, image, raytrace_buffer, rt_resolve_input, rt_resolve_output[back_buffer.index]);
			}*/

			cmd_list.finish();
		}
		cmd_queue.submit_command_lists({ &cmd_list }, nullptr);

		++frame_index;

		if (cmd_queue.present(swapchain) != gfx::swap_chain_status::ok) {
			resized = true;
		}

		{ // wait until the previous frame has finished
			gfx::fence frame_fence = dev.create_fence(gfx::synchronization_state::unset);
			cmd_queue.signal(frame_fence);
			dev.wait_for_fence(frame_fence);
		}
	}

	return 0;
}
