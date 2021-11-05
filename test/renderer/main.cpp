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

#include "common.h"
#include "scene.h"
#include "gbuffer_pass.h"
#include "composite_pass.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "No model file specified\n";
		return 1;
	}

	std::cout << "Backend: " << std::string_view(reinterpret_cast<const char*>(gfx::backend_name.data()), gfx::backend_name.size()) << "\n";
	std::cout << "Working dir: " << std::filesystem::current_path() << "\n";

	sys::application app(u8"test");
	auto wnd = app.create_window();

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


	// model & resources
	gltf::Model model;
	{
		gltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		std::cout << "Loading " << argv[1] << "\n";
		if (!loader.LoadASCIIFromFile(&model, &err, &warn, argv[1])) {
			std::cout << "Failed to load scene: " << err << "\n";
		}
		std::cout << warn << "\n";
	}

	auto descriptor_pool = dev.create_descriptor_pool(
		{
			gfx::descriptor_range::create(gfx::descriptor_type::read_only_image, 100),
			gfx::descriptor_range::create(gfx::descriptor_type::sampler, 100),
		}, 100
	);
	auto sampler = dev.create_sampler(
		gfx::filtering::linear, gfx::filtering::linear, gfx::filtering::linear, 0, 0, 1, 16.0f,
		gfx::sampler_address_mode::repeat, gfx::sampler_address_mode::repeat, gfx::sampler_address_mode::repeat,
		linear_rgba_f(1.f, 1.f, 1.f, 1.f), std::nullopt
	);

	auto model_resources = scene_resources::create(dev, dev_prop, cmd_alloc, cmd_queue, descriptor_pool, sampler, model);

	gbuffer_pass gbuf_pass(dev, model_resources.material_descriptor_layout, model_resources.node_descriptor_layout);
	composite_pass comp_pass(dev);

	// buffers
	gbuffer gbuf;
	gbuffer::view gbuf_view;
	gbuffer_pass::input_resources gbuf_input = gbuf_pass.create_input_resources(dev, dev_prop, descriptor_pool, model, model_resources);
	gbuffer_pass::output_resources gbuf_output;

	composite_pass::input_resources comp_input;
	std::vector<composite_pass::output_resources> comp_output;

	gfx::swap_chain swapchain = nullptr;
	std::vector<gfx::fence> present_fences;
	std::vector<gfx::command_list> cmd_lists;

	auto recreate_buffers = [&](cvec2s size) {
		auto beg = std::chrono::high_resolution_clock::now();

		// reset all textures
		gbuf = gbuffer();
		gbuf_view = gbuffer::view();
		swapchain = nullptr;
		comp_output.clear();
		present_fences.clear();

		// create all textures
		gbuf = gbuffer::create(dev, cmd_alloc, cmd_queue, size);
		gbuf_view = gbuf.create_view(dev);
		auto [sc, fmt] = ctx.create_swap_chain_for_window(
			wnd, dev, cmd_queue, 2, { gfx::format::r8g8b8a8_srgb, gfx::format::b8g8r8a8_srgb }
		);
		swapchain = std::move(sc);
		auto time = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - beg).count();

		// create input/output resources
		gbuf_output = gbuf_pass.create_output_resources(dev, gbuf_view, size);

		auto list = dev.create_and_start_command_list(cmd_alloc);

		comp_input = comp_pass.create_input_resources(dev, descriptor_pool, gbuf_view);
		std::size_t num_images = swapchain.get_image_count();
		cmd_lists.clear();
		for (std::size_t i = 0; i < num_images; ++i) {
			auto image = swapchain.get_image(i);
			char8_t buffer[20];
			std::snprintf(reinterpret_cast<char*>(buffer), std::size(buffer), "Back buffer %d", static_cast<int>(i));
			dev.set_debug_name(image, buffer);
			comp_output.emplace_back(comp_pass.create_output_resources(dev, image, fmt, size));
			present_fences.emplace_back(dev.create_fence(gfx::synchronization_state::unset));
			cmd_lists.emplace_back(nullptr);
			list.resource_barrier(
				{
					gfx::image_barrier::create(gfx::subresource_index::first_color(), image, gfx::image_usage::initial, gfx::image_usage::present),
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
		bool rotate = false;
		bool zoom = false;
		bool move = false;
		cvec2i prev_mouse = uninitialized;
	} cam_closure = { cam_params, cam };
	auto mouse_move_node = wnd.on_mouse_move.create_linked_node(
		[&cam_closure](sys::window&, sys::window_events::mouse::move &move) {
			cvec2f offset = (move.new_position - cam_closure.prev_mouse).into<float>();
			offset[0] = -offset[0];
			if (cam_closure.rotate) {
				cam_closure.cam_params.rotate_around_world_up(offset * 0.004f);
			}
			if (cam_closure.zoom) {
				cvec3f cam_offset = cam_closure.cam_params.position - cam_closure.cam_params.look_at;
				cam_offset *= std::exp(-0.005f * offset[1]);
				cam_closure.cam_params.position = cam_closure.cam_params.look_at + cam_offset;
			}
			if (cam_closure.move) {
				cvec3f x = cam_closure.cam.unit_right * offset[0];
				cvec3f y = cam_closure.cam.unit_up * offset[1];
				float distance = (cam_closure.cam_params.position - cam_closure.cam_params.look_at).norm() * 0.001f;
				cvec3f cam_off = (x + y) * distance;
				cam_closure.cam_params.position += cam_off;
				cam_closure.cam_params.look_at += cam_off;
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
		[&app](sys::window&, sys::window_events::close_request&) {
			app.quit();
		}
	);



	wnd.show_and_activate();
	while (app.process_message_nonblocking() != sys::message_type::quit) {
		if (resized) {
			recreate_buffers(wnd.get_size());
			resized = false;
		}

		auto back_buffer = dev.acquire_back_buffer(swapchain);

		if (back_buffer.on_presented) {
			dev.wait_for_fence(*back_buffer.on_presented);
			dev.reset_fence(*back_buffer.on_presented);
		}

		cam = cam_params.into_camera();

		auto *data = static_cast<gbuffer_pass::constants*>(dev.map_buffer(gbuf_input.constant_buffer, 0, 0));
		data->view = cam.view_matrix.into<float>();
		data->projection_view = cam.projection_view_matrix.into<float>();
		dev.unmap_buffer(gbuf_input.constant_buffer, 0, sizeof(gbuffer_pass::constants));

		auto &cmd_list = cmd_lists[back_buffer.index];
		cmd_list = dev.create_and_start_command_list(cmd_alloc);
		{
			auto image = swapchain.get_image(back_buffer.index);
			
			gbuf_pass.record_commands(cmd_list, gbuf, model, model_resources, gbuf_input, gbuf_output);
			comp_pass.record_commands(cmd_list, image, comp_input, comp_output[back_buffer.index]);

			cmd_list.finish();
		}
		cmd_queue.submit_command_lists({ &cmd_list }, nullptr);

		cmd_queue.present(swapchain);

		{ // wait until the previous frame has finished
			gfx::fence frame_fence = dev.create_fence(gfx::synchronization_state::unset);
			cmd_queue.signal(frame_fence);
			dev.wait_for_fence(frame_fence);
		}
	}

	{
		auto fence = dev.create_fence(gfx::synchronization_state::unset);
		cmd_queue.signal(fence);
		dev.wait_for_fence(fence);
	}

	return 0;
}
