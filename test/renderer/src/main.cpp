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
/*#include "gbuffer_pass.h"
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
	auto gctx = gfx::context::create();
	gfx::device gdev = nullptr;
	gfx::adapter_properties dev_prop = uninitialized;
	gctx.enumerate_adapters([&](gfx::adapter adap) {
		dev_prop = adap.get_properties();
		std::cerr << "Device name: " << reinterpret_cast<const char*>(dev_prop.name.c_str()) << "\n";
		if (dev_prop.is_discrete) {
			std::cerr << "  Selected\n";
			gdev = adap.create_device();
			return false;
		}
		return true;
	});
	auto cmd_queue = gdev.create_command_queue();
	auto cmd_alloc = gdev.create_command_allocator();

	auto rctx = ren::context::create(gctx, gdev, cmd_queue);
	auto asset_man = ren::assets::manager::create(rctx, gdev, cmd_queue, "D:/Documents/Projects/lotus/lotus/renderer/include/lotus/renderer/shaders", &shader_util);

	// model & resources
	auto gltf_ctx = ren::gltf::context::create(asset_man);
	auto instances = gltf_ctx.load(argv[1]);

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

	auto blit_vs = asset_man.compile_shader_in_filesystem(
		asset_man.get_shader_library_path() / "fullscreen_quad_vs.hlsl", gfx::shader_stage::vertex_shader, u8"main_vs", {}
	);
	auto blit_ps = asset_man.compile_shader_in_filesystem(
		asset_man.get_shader_library_path() / "blit_ps.hlsl", gfx::shader_stage::pixel_shader, u8"main_ps", {}
	);

	auto swap_chain = rctx.request_swap_chain(
		u8"Main swap chain", wnd, 2, { gfx::format::r8g8b8a8_srgb, gfx::format::b8g8r8a8_srgb }
	);
	std::size_t frame_index = 0;

	auto recreate_buffers = [&](cvec2s size) {
		frame_index = 0;
	};


	auto cam_params = camera_parameters<float>::create_look_at(cvec3f(0.0f, 100.0f, 0.0f), cvec3f(500.0f, 100.0f, 0.0f));
	{
		auto size = wnd.get_size();
		cam_params.far_plane = 10000.0f;
		cam_params.aspect_ratio = size[0] / static_cast<float>(size[1]);
	}
	auto cam = cam_params.into_camera();
	bool is_rotating = false;
	bool is_zooming = false;
	bool is_moving = false;
	cvec2i prev_mouse = zero;
	cvec2s window_size = zero;

	auto on_resize = [&](sys::window&, sys::window_events::resize &info) {
		window_size = info.new_size;
		frame_index = 0;
		swap_chain.resize(info.new_size);
		cam_params.aspect_ratio = info.new_size[0] / static_cast<float>(info.new_size[1]);
	};
	auto on_mouse_move = [&](sys::window&, sys::window_events::mouse::move &move) {
		cvec2f offset = (move.new_position - prev_mouse).into<float>();
		offset[0] = -offset[0];
		if (is_rotating) {
			cam_params.rotate_around_world_up(offset * 0.004f);
			frame_index = 0;
		}
		if (is_zooming) {
			cvec3f cam_offset = cam_params.position - cam_params.look_at;
			cam_offset *= std::exp(-0.005f * offset[1]);
			cam_params.position = cam_params.look_at + cam_offset;
			frame_index = 0;
		}
		if (is_moving) {
			cvec3f x = cam.unit_right * offset[0];
			cvec3f y = cam.unit_up * offset[1];
			float distance = (cam_params.position - cam_params.look_at).norm() * 0.001f;
			cvec3f cam_off = (x + y) * distance;
			cam_params.position += cam_off;
			cam_params.look_at += cam_off;
			frame_index = 0;
		}
		prev_mouse = move.new_position;
	};
	auto on_mouse_down = [&](sys::window &wnd, sys::window_events::mouse::button_down &down) {
		switch (down.button) {
		case sys::mouse_button::primary:
			is_rotating = true;
			break;
		case sys::mouse_button::secondary:
			is_zooming = true;
			break;
		case sys::mouse_button::middle:
			is_moving = true;
			break;
		}
		wnd.acquire_mouse_capture();
	};
	auto on_mouse_up = [&](sys::window &wnd, sys::window_events::mouse::button_up &up) {
		switch (up.button) {
		case sys::mouse_button::primary:
			is_rotating = false;
			break;
		case sys::mouse_button::secondary:
			is_zooming = false;
			break;
		case sys::mouse_button::middle:
			is_moving = false;
			break;
		}
		if (!is_rotating && !is_zooming && !is_moving) {
			wnd.release_mouse_capture();
		}
	};
	auto on_capture_broken = [&](sys::window&) {
		is_rotating = is_zooming = is_moving = false;
	};

	auto size_node = wnd.on_resize.create_linked_node(
		[&](sys::window &wnd, sys::window_events::resize &info) {
			on_resize(wnd, info);
		}
	);
	auto mouse_move_node = wnd.on_mouse_move.create_linked_node(
		[&](sys::window &wnd, sys::window_events::mouse::move &move) {
			on_mouse_move(wnd, move);
		}
	);
	auto mouse_down_node = wnd.on_mouse_button_down.create_linked_node(
		[&](sys::window &wnd, sys::window_events::mouse::button_down &down) {
			on_mouse_down(wnd, down);
		}
	);
	auto mouse_up_node = wnd.on_mouse_button_up.create_linked_node(
		[&](sys::window &wnd, sys::window_events::mouse::button_up &up) {
			on_mouse_up(wnd, up);
		}
	);
	auto capture_broken_node = wnd.on_capture_broken.create_linked_node(
		[&](sys::window &wnd) {
			on_capture_broken(wnd);
		}
	);
	auto quit_node = wnd.on_close_request.create_linked_node(
		[&](sys::window&, sys::window_events::close_request &req) {
			req.should_close = true;
			app.quit();
		}
	);


	wnd.show_and_activate();
	while (app.process_message_nonblocking() != sys::message_type::quit) {
		if (window_size == zero) {
			continue;
		}

		{
			cam = cam_params.into_camera();

			auto gbuffer = ren::g_buffer::view::create(rctx, window_size);
			{
				auto pass = gbuffer.begin_pass(rctx);
				ren::g_buffer::render_instances(pass, asset_man, instances, cam.projection_view_matrix);
				pass.end();
			}

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

			/*if constexpr (0) {
				gbuf_pass.record_commands(cmd_list, gbuf, model, model_resources, gbuf_input, gbuf_output);
				comp_pass.record_commands(cmd_list, image, comp_input, comp_output[back_buffer.index]);
			} else {
				rt_pass.record_commands(cmd_list, model, model_resources, rt_input, raytrace_buffer);
				rt_resolve_pass.record_commands(cmd_list, image, raytrace_buffer, rt_resolve_input, rt_resolve_output[back_buffer.index]);
			}*/

			{
				auto pass = rctx.begin_pass({
					ren::surface2d_color(
						swap_chain,
						gfx::color_render_target_access::create_clear(cvec4d(0.0f, 0.0f, 0.0f, 0.0f))
					)
				}, nullptr, window_size, u8"Final blit");
				ren::graphics_pipeline_state state(
					{
						gfx::render_target_blend_options::disabled()
					},
					nullptr,
					nullptr
				);
				pass.draw_instanced(
					{}, 3, nullptr, 0, gfx::primitive_topology::triangle_list,
					ren::all_resource_bindings::from_unsorted({
						ren::resource_set_binding::create({
							ren::resource_binding(ren::descriptor_resource::image2d(
								gbuffer.albedo_glossiness, ren::image_binding_type::read_only
							), 0),
							ren::resource_binding(ren::descriptor_resource::sampler(), 1),
						}, 0)
					}),
					blit_vs, blit_ps, state, 1, u8"Final blit"
				);
				pass.end();
			}

			rctx.present(swap_chain, u8"Present");
		}

		rctx.flush();

		++frame_index;
	}

	return 0;
}
