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

#include "application.h"

#include <imgui.cpp>
#include <imgui_draw.cpp>
#include <imgui_widgets.cpp>
#include <imgui_tables.cpp>

/*#define NO_SCENES*/
//#define DISABLE_ALL_RT

class path_tracer_app : public lotus::application {
public:
	path_tracer_app(int argc, char **argv, lgpu::context_options gpu_context_opts) :
		application(argc, argv, u8"Path Tracer", gpu_context_opts) {
	}

	std::unique_ptr<scene_representation> scene;

	u32 frame_index = 0;

	lren::pool runtime_buf_pool = nullptr;
	lren::pool runtime_tex_pool = nullptr;

	lren::assets::handle<lren::assets::shader_library> rt_shader = nullptr;
	lren::assets::handle<lren::assets::shader> blit_vs = nullptr;
	lren::assets::handle<lren::assets::shader> blit_ps = nullptr;
	lren::assets::handle<lren::assets::shader> resolve_ps = nullptr;

	lotus::camera_parameters<float> cam_params = uninitialized;
	lotus::camera_control<float> cam_control = nullptr;

	lren::image2d_view rt_result = nullptr;
private:
	static constexpr lgpu::queue_family _queues[] = { lgpu::queue_family::graphics, lgpu::queue_family::copy };

	lren::context::queue _gfx_q = nullptr;

	std::span<const lgpu::queue_family> _get_desired_queues() const override {
		return _queues;
	}
	u32 _get_asset_loading_queue_index() const override {
		return 1;
	}
	u32 _get_constant_upload_queue_index() const override {
		return 1;
	}
	u32 _get_debug_drawing_queue_index() const override {
		return 0;
	}
	u32 _get_present_queue_index() const override {
		return 0;
	}
	std::filesystem::path _get_asset_library_path() const override {
		//return "D:/Documents/Projects/lotus/lotus/renderer/include/lotus/renderer/assets";
		return "/Users/xuanyizhou/Documents/Projects/lotus/lotus/renderer/include/lotus/renderer/assets";
	}
	std::vector<std::filesystem::path> _get_additional_shader_include_paths() const override {
		return {
			_assets->asset_library_path / "shaders",
			//"D:/Documents/Projects/lotus/test/renderer/common/include",
			"/Users/xuanyizhou/Documents/Projects/lotus/test/renderer/common/include",
		};
	}

	void _on_initialized() override {
		_gfx_q = _context->get_queue(0);

		scene = std::make_unique<scene_representation>(*_assets, _gfx_q);
		for (int i = 1; i < _argc; ++i) {
			scene->load(_argv[i]);
		}
		scene->finish_loading();

		runtime_buf_pool = _context->request_pool(u8"Run-time Buffers");
		runtime_tex_pool = _context->request_pool(u8"Run-time Textures");

		rt_shader  = _assets->compile_shader_library_in_filesystem("src/shaders/raytracing.hlsl", {});
		blit_vs    = _assets->compile_shader_in_filesystem(_assets->asset_library_path / "shaders/misc/fullscreen_quad_vs.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs", {});
		blit_ps    = _assets->compile_shader_in_filesystem(_assets->asset_library_path / "shaders/misc/blit_ps.hlsl",            lgpu::shader_stage::pixel_shader,  u8"main_ps", {});
		resolve_ps = _assets->compile_shader_in_filesystem("src/shaders/rt_resolve.hlsl",                                        lgpu::shader_stage::pixel_shader,  u8"main_ps", {});

		cam_params = lotus::camera_parameters<float>::create_look_at(cvec3f(0.0f, 0.0f, 0.0f), cvec3f(50.0f, 10.0f, 0.0f));
		cam_params.far_plane = 4000.0f;
		cam_control = lotus::camera_control<float>(cam_params);
	}

	void _on_resize(lsys::window_events::resize &resize) override {
		frame_index = 0;
		const cvec2u32 sz = resize.new_size;
		cam_params.aspect_ratio = sz[0] / static_cast<float>(sz[1]);
		rt_result = _context->request_image2d(
			u8"Raytracing result", sz, 1, lgpu::format::r32g32b32a32_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);
	}

	void _on_mouse_move(lsys::window_events::mouse::move &move) override {
		if (cam_control.on_mouse_move(move.new_position)) {
			frame_index = 0;
		}
	}
	void _on_mouse_down(lsys::window_events::mouse::button_down &down) override {
		if (cam_control.on_mouse_down(down.button, down.modifiers)) {
			_window->acquire_mouse_capture();
		}
	}
	void _on_mouse_up(lsys::window_events::mouse::button_up &up) override {
		if (cam_control.on_mouse_up(up.button)) {
			_window->release_mouse_capture();
		}
	}
	void _on_capture_broken() override {
		cam_control.on_capture_broken();
	}

	void _process_frame(lotus::renderer::constant_uploader &uploader, lotus::renderer::dependency constants_dep, lotus::renderer::dependency assets_dep) override {
		_gfx_q.acquire_dependency(constants_dep, u8"Wait for constants");
		if (assets_dep) {
			_gfx_q.acquire_dependency(assets_dep, u8"Wait for assets");
		}

		const lotus::camera<float> cam = cam_params.into_camera();
		const cvec2u32 window_size = _get_window_size();

		float tan_half_fovy = tan(cam_params.fov_y_radians * 0.5f);
		auto right_half = cam.unit_right * tan_half_fovy * cam_params.aspect_ratio;
		auto up_half = cam.unit_up * tan_half_fovy;

		shader_types::global_data globals;
		globals.camera_position = cam_params.position;
		globals.t_min = 0.001f;
		globals.top_left = cam.unit_forward - right_half + up_half;
		globals.t_max = std::numeric_limits<float>::max();
		globals.right = right_half / (window_size[0] * 0.5f);
		globals.down = -up_half / (window_size[1] * 0.5f);
		globals.frame_index = frame_index;

		{
			lren::all_resource_bindings resources(
				{
					{ 0, {
						{ 0, scene->tlas },
						{ 1, uploader.upload(globals) },
						{ 2, rt_result.bind_as_read_write() },
						{ 3, lren::sampler_state() },
					} },
					{ 1, _assets->get_images() },
					{ 2, scene->vertex_buffers },
					{ 3, scene->normal_buffers },
					{ 4, scene->tangent_buffers },
					{ 5, scene->uv_buffers },
					{ 6, scene->index_buffers },
					{ 7, {
						{ 0, scene->instances_buffer.bind_as_read_only() },
						{ 1, scene->geometries_buffer.bind_as_read_only() },
						{ 2, scene->materials_buffer.bind_as_read_only() },
					} },
				},
				{}
			);
			_gfx_q.trace_rays(
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
				cvec3u32(window_size, 1u),
				std::move(resources),
				u8"Trace rays"
			);
		}

		{
			auto pass = _gfx_q.begin_pass({
				lren::image2d_color(
					_swap_chain,
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
							{ 2, uploader.upload(globals) },
						} },
					},
					{}
				),
				blit_vs, resolve_ps, state, 1, u8"Final blit"
			);
			pass.end();
		}

		++frame_index;
	}
};

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "No model file specified\n";
		return 1;
	}
	path_tracer_app app(argc, argv, lotus::gpu::context_options::enable_validation | lotus::gpu::context_options::enable_debug_info);
	app.initialize();
	return app.run();
}
