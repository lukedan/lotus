#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

#include <lotus/math/vector.h>
#include <lotus/utils/camera.h>
#include <lotus/utils/misc.h>

#include <lotus/system/window.h>
#include <lotus/system/application.h>

#include <lotus/gpu/context.h>
#include <lotus/gpu/commands.h>
#include <lotus/gpu/descriptors.h>

#include <lotus/renderer/context/asset_manager.h>
#include <lotus/renderer/context/context.h>

#include <lotus/renderer/mipmap.h>

#include <lotus/renderer/loaders/assimp_loader.h>
#include <lotus/renderer/loaders/gltf_loader.h>
#include <lotus/renderer/loaders/fbx_loader.h>

#include <scene.h>
#include <camera_control.h>

#include <lotus/renderer/shader_types_include_wrapper.h>
namespace shader_types {
#include "shaders/types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>

/*#define NO_SCENES*/
//#define DISABLE_ALL_RT

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "No model file specified\n";
		return 1;
	}

	log().debug("Backend: {}", lstr::to_generic(lgpu::get_backend_name(lgpu::current_backend)));
	log().debug("Working dir: {}", std::filesystem::current_path().string());

	lsys::application app(u8"test");
	auto wnd = app.create_window();

	auto gctx_options = true ? lgpu::context_options::enable_validation : lgpu::context_options::none;
	auto gctx = lgpu::context::create(gctx_options);
	auto shader_util = lgpu::shader_utility::create();
	lgpu::device gdev = nullptr;
	std::vector<lgpu::command_queue> gqueues;
	lgpu::adapter_properties dev_prop = uninitialized;
	gctx.enumerate_adapters([&](lgpu::adapter adap) {
		dev_prop = adap.get_properties();
		log().debug("Device name: {}", lstr::to_generic(dev_prop.name));
		if (dev_prop.is_discrete) {
			log().debug("Selected");
			auto &&[dev, queues] = adap.create_device({ lgpu::queue_family::graphics });
			gdev = std::move(dev);
			gqueues = std::move(queues);
			return false;
		}
		return true;
	});
	auto cmd_alloc = gdev.create_command_allocator(lgpu::queue_family::graphics);

	auto rctx = lren::context::create(gctx, dev_prop, gdev, gqueues);
	auto gfx_q = rctx.get_queue(0);
	auto asset_man = lren::assets::manager::create(rctx, gfx_q, &shader_util);
	asset_man.asset_library_path = "D:/Documents/Projects/lotus/lotus/renderer/include/lotus/renderer/assets/";
	asset_man.additional_shader_include_paths = {
		asset_man.asset_library_path / "shaders/",
		"D:/Documents/Projects/lotus/test/renderer/common/include/",
	};

	auto runtime_buf_pool = rctx.request_pool(u8"Run-time Buffers");
	auto runtime_tex_pool = rctx.request_pool(u8"Run-time Textures");

	auto mip_gen = lren::mipmap::generator::create(asset_man, gfx_q);
	lren::gltf::context gltf_ctx(asset_man);
	auto fbx_ctx = lren::fbx::context::create(asset_man);
	lren::assimp::context assimp_ctx(asset_man);

	// model & resources
#ifndef NO_SCENES
	scene_representation scene(asset_man, gfx_q);

	for (int i = 1; i < argc; ++i) {
		scene.load(argv[i]);
	}
	scene.finish_loading();
#endif

#ifndef DISABLE_ALL_RT
	auto rt_shader = asset_man.compile_shader_library_in_filesystem("src/shaders/raytracing.hlsl", {});
#endif

	auto blit_vs = asset_man.compile_shader_in_filesystem(
		asset_man.asset_library_path / "shaders/misc/fullscreen_quad_vs.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs", {}
	);
	auto blit_ps = asset_man.compile_shader_in_filesystem(
		asset_man.asset_library_path / "shaders/misc/blit_ps.hlsl", lgpu::shader_stage::pixel_shader, u8"main_ps", {}
	);

	auto resolve_ps = asset_man.compile_shader_in_filesystem("src/shaders/rt_resolve.hlsl", lgpu::shader_stage::pixel_shader, u8"main_ps", {});

	auto swap_chain = rctx.request_swap_chain(
		u8"Main swap chain", wnd, gfx_q, 2, { lgpu::format::r8g8b8a8_srgb, lgpu::format::b8g8r8a8_srgb }
	);
	std::size_t frame_index = 0;

	auto cam_params = lotus::camera_parameters<float>::create_look_at(cvec3f(0.0f, 0.0f, 0.0f), cvec3f(50.0f, 10.0f, 0.0f));
	{
		auto size = wnd.get_size();
		cam_params.far_plane = 4000.0f;
		cam_params.aspect_ratio = size[0] / static_cast<float>(size[1]);
	}
	auto cam = cam_params.into_camera();
	lotus::camera_control<float> cam_control(cam_params);

	cvec2u32 window_size = zero;
#ifndef DISABLE_ALL_RT
	lren::image2d_view rt_result = nullptr;
#endif

	auto on_resize = [&](lsys::window_events::resize &info) {
		window_size = info.new_size;
		frame_index = 0;
		swap_chain.resize(info.new_size);
		cam_params.aspect_ratio = info.new_size[0] / static_cast<float>(info.new_size[1]);
#ifndef DISABLE_ALL_RT
		rt_result = rctx.request_image2d(
			u8"Raytracing result", window_size, 1, lgpu::format::r32g32b32a32_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);
#endif
	};
	wnd.on_resize = [&](lsys::window_events::resize &info) {
		on_resize(info);
	};

	auto on_mouse_move = [&](lsys::window_events::mouse::move &move) {
		if (cam_control.on_mouse_move(move.new_position)) {
			frame_index = 0;
		}
	};
	wnd.on_mouse_move = [&](lsys::window_events::mouse::move &move) {
		on_mouse_move(move);
	};

	auto on_mouse_down = [&](lsys::window_events::mouse::button_down &down) {
		if (cam_control.on_mouse_down(down.button)) {
			wnd.acquire_mouse_capture();
		}
	};
	wnd.on_mouse_button_down = [&](lsys::window_events::mouse::button_down &down) {
		on_mouse_down(down);
	};

	auto on_mouse_up = [&](lsys::window_events::mouse::button_up &up) {
		if (cam_control.on_mouse_up(up.button)) {
			wnd.release_mouse_capture();
		}
	};
	wnd.on_mouse_button_up = [&](lsys::window_events::mouse::button_up &up) {
		on_mouse_up(up);
	};

	auto on_capture_broken = [&]() {
		cam_control.on_capture_broken();
	};
	wnd.on_capture_broken = [&]() {
		on_capture_broken();
	};

	wnd.on_close_request = [&](lsys::window_events::close_request &req) {
		req.should_close = true;
		app.quit();
	};


	wnd.show_and_activate();
	while (app.process_message_nonblocking() != lsys::message_type::quit) {
		if (window_size == zero) {
			continue;
		}

		auto start = std::chrono::high_resolution_clock::now();

		{
			asset_man.update();

			cam = cam_params.into_camera();

			float tan_half_fovy = tan(cam_params.fov_y_radians * 0.5);
			auto right_half = cam.unit_right * tan_half_fovy * cam_params.aspect_ratio;
			auto up_half = cam.unit_up * tan_half_fovy;

			shader_types::global_data globals;
			globals.camera_position = cam_params.position;
			globals.t_min = 0.001f;
			globals.top_left = cam.unit_forward - right_half + up_half;
			globals.t_max = std::numeric_limits<float>::max();
			globals.right = right_half / (window_size[0] * 0.5f);
			globals.down = -up_half / (window_size[1] * 0.5f);
#ifdef DISABLE_ALL_RT
			globals.frame_index = 1;
#else
			globals.frame_index = frame_index;
#endif

#ifndef DISABLE_ALL_RT
			{
				lren::all_resource_bindings resources(
					{
						{ 0, {
							{ 0, scene.tlas },
							{ 1, lren::descriptor_resource::immediate_constant_buffer::create_for(globals) },
							{ 2, rt_result.bind_as_read_write() },
							{ 3, lren::sampler_state() },
						} },
						{ 1, asset_man.get_images() },
						{ 2, scene.vertex_buffers },
						{ 3, scene.normal_buffers },
						{ 4, scene.tangent_buffers },
						{ 5, scene.uv_buffers },
						{ 6, scene.index_buffers },
						{ 7, {
							{ 0, scene.instances_buffer.bind_as_read_only() },
							{ 1, scene.geometries_buffer.bind_as_read_only() },
							{ 2, scene.materials_buffer.bind_as_read_only() },
						} },
					},
					{}
				);
				gfx_q.trace_rays(
					{
						lren::shader_function(rt_shader, u8"main_anyhit_indexed", lgpu::shader_stage::any_hit_shader),
						lren::shader_function(rt_shader, u8"main_anyhit_unindexed", lgpu::shader_stage::any_hit_shader),
						lren::shader_function(rt_shader, u8"main_closesthit_indexed", lgpu::shader_stage::closest_hit_shader),
						lren::shader_function(rt_shader, u8"main_closesthit_unindexed", lgpu::shader_stage::closest_hit_shader),
					},
					{
						lgpu::hit_shader_group(2, 0),
						lgpu::hit_shader_group(3, 1),
					},
					{
						lren::shader_function(rt_shader, u8"main_raygen", lgpu::shader_stage::ray_generation_shader),
						lren::shader_function(rt_shader, u8"main_miss", lgpu::shader_stage::miss_shader),
					},
					2, { 3 }, { 0, 1 },
					20, 32, 32,
					cvec3u32(window_size[0], window_size[1], 1),
					std::move(resources),
					u8"Trace rays"
				);
			}
#endif

			{
				auto pass = gfx_q.begin_pass({
					lren::image2d_color(
						swap_chain,
						lgpu::color_render_target_access::create_clear(cvec4d(0.0f, 0.0f, 0.0f, 0.0f))
					)
				}, nullptr, window_size, u8"Final blit");
				lren::graphics_pipeline_state state(
					{
						lgpu::render_target_blend_options::disabled()
					},
					nullptr,
					nullptr
				);
				pass.draw_instanced(
					{}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list,
					lren::all_resource_bindings(
						{
							{ 0, {
								{ 0, rt_result.bind_as_read_only() },
								{ 1, lren::sampler_state() },
								{ 2, lren::descriptor_resource::immediate_constant_buffer::create_for(globals) },
							} },
						},
						{}
					),
					blit_vs, resolve_ps, state, 1, u8"Final blit"
				);
				pass.end();
			}

			gfx_q.present(swap_chain, u8"Present");
		}

		rctx.execute_all();

		++frame_index;

		auto end = std::chrono::high_resolution_clock::now();

		wnd.set_title(lstr::assume_utf8(std::format(
			"Frame {}, Frame Time: {}", frame_index, std::chrono::duration<float, std::milli>(end - start).count()
		)));
	}

	return 0;
}
