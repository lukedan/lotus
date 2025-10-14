#include <cinttypes>
#include <queue>
#include <random>

#include <scene.h>
#include <camera_control.h>

#include <lotus/math/sequences.h>
#include <lotus/utils/camera.h>
#include <lotus/renderer/g_buffer.h>
#include <lotus/helpers/application.h>

#include <lotus/renderer/shader_types_include_wrapper.h>
namespace shader_types {
#include "shaders/shader_types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>

#include <imgui.cpp>
#include <imgui_draw.cpp>
#include <imgui_widgets.cpp>
#include <imgui_tables.cpp>

template <typename T> struct ImGuiAutoDataType;
template <> struct ImGuiAutoDataType<u32> {
	constexpr static ImGuiDataType value = ImGuiDataType_U32;
};
template <typename T> constexpr static ImGuiDataType ImGuiAutoDataType_v = ImGuiAutoDataType<T>::value;

template <typename T> static bool ImGui_SliderT(const char *label, T *data, T min, T max, const char *format = nullptr, ImGuiSliderFlags flags = 0) {
	return ImGui::SliderScalar(label, ImGuiAutoDataType_v<T>, data, &min, &max, format, flags);
}

class restir_probe_app : public lotus::helpers::application {
public:
	restir_probe_app(int argc, char **argv) : application(argc, argv, u8"ReSTIR Probes") {
	}


	std::unique_ptr<scene_representation> scene;

	u32 frame_index = 0;

	lren::pool runtime_tex_pool = nullptr;
	lren::pool runtime_buf_pool = nullptr;

	lren::assets::handle<lren::assets::shader> fs_quad_vs           = nullptr;
	lren::assets::handle<lren::assets::shader> blit_ps              = nullptr;
	lren::assets::handle<lren::assets::shader> fill_buffer_cs       = nullptr;
	lren::assets::handle<lren::assets::shader> fill_texture3d_cs    = nullptr;
	lren::assets::handle<lren::assets::shader> show_gbuffer_ps      = nullptr;
	lren::assets::handle<lren::assets::shader> visualize_probes_vs  = nullptr;
	lren::assets::handle<lren::assets::shader> visualize_probes_ps  = nullptr;
	lren::assets::handle<lren::assets::shader> shade_point_debug_cs = nullptr;

	lren::assets::handle<lren::assets::shader> direct_update_cs          = nullptr;
	lren::assets::handle<lren::assets::shader> indirect_update_cs        = nullptr;
	lren::assets::handle<lren::assets::shader> summarize_probes_cs       = nullptr;
	lren::assets::handle<lren::assets::shader> indirect_spatial_reuse_cs = nullptr;
	lren::assets::handle<lren::assets::shader> indirect_specular_cs      = nullptr;
	lren::assets::handle<lren::assets::shader> indirect_specular_vndf_cs = nullptr;
	lren::assets::handle<lren::assets::shader> lighting_cs               = nullptr;
	lren::assets::handle<lren::assets::shader> sky_vs                    = nullptr;
	lren::assets::handle<lren::assets::shader> sky_ps                    = nullptr;
	lren::assets::handle<lren::assets::shader> taa_cs                    = nullptr;
	lren::assets::handle<lren::assets::shader> lighting_blit_ps          = nullptr;

	lren::assets::handle<lren::assets::image2d> envmap_lut = nullptr;
	lren::assets::handle<lren::assets::image2d> sky_hdri   = nullptr;

	lotus::camera_parameters<f32> cam_params = uninitialized;
	lotus::camera<f32> prev_cam = uninitialized;
	lotus::camera_control<f32> cam_control = nullptr;

	f32 lighting_scale = 1.0f;
	int lighting_mode = 1;
	char sky_hdri_path[1024] = { 0 };
	f32 sky_scale = 1.0f;
	cvec3u32 probe_density = cvec3u32(50u, 50u, 50u);
	u32 direct_reservoirs_per_probe = 2;
	u32 indirect_reservoirs_per_probe = 4;
	u32 direct_sample_count_cap = 10;
	u32 indirect_sample_count_cap = 10;
	u32 indirect_spatial_reuse_passes = 3;
	lotus::aab3f32 probe_bounds = lotus::aab3f32::create_from_min_max({ -10.0f, -10.0f, -10.0f }, { 10.0f, 10.0f, 10.0f });
	f32 visualize_probe_size = 0.1f;
	int visualize_probes_mode = 0;
	int shade_point_debug_mode = 0;
	bool trace_shadow_rays_naive = true;
	bool trace_shadow_rays_reservoir = false;
	f32 diffuse_mul = 1.0f;
	f32 specular_mul = 1.0f;
	f32 sh_ra_factor = 0.05f;
	bool use_indirect_diffuse = true;
	bool use_indirect_specular = true;
	bool indirect_specular_use_visible_normals = true;
	bool enable_indirect_specular_mis = true;
	bool use_ss_indirect_specular = true;
	bool approx_indirect_indirect_specular = true;
	bool debug_approx_for_indirect = false;
	bool update_probes = true;
	bool update_probes_this_frame = false;
	bool indirect_temporal_reuse = true;
	bool indirect_spatial_reuse = true;
	int indirect_spatial_reuse_visibility_test_mode = 1;
	int gbuffer_visualization = 0;

	bool enable_taa = true;
	f32 taa_ra_factor = 0.1f;
	int taa_sequence_x = 1;
	int taa_sequence_y = 1;
	u32 taa_sample_count = 8;
	u32 taa_sample_offset = 17;
	u32 taa_sample_param_x = 2;
	u32 taa_sample_param_y = 3;

	std::vector<cvec2f32> taa_samples;
	u32 taa_phase = 0;

	u32 num_accumulated_frames = 0;

	shader_types::probe_constants probe_constants;

	lren::image2d_view path_tracer_accum = nullptr;
	lren::image2d_view prev_irradiance = nullptr;

	lren::structured_buffer_view direct_reservoirs = nullptr;
	lren::structured_buffer_view indirect_reservoirs = nullptr;
	lren::image3d_view probe_sh0 = nullptr;
	lren::image3d_view probe_sh1 = nullptr;
	lren::image3d_view probe_sh2 = nullptr;
	lren::image3d_view probe_sh3 = nullptr;
	bool clear_reservoirs = false;

	std::default_random_engine rng;


	static f32 get_taa_sample(int mode, int index, int param) {
		switch (mode) {
		case 0:
			return 0.5f;
		case 1:
			{
				auto seq = lotus::sequences::halton<f32>::create(param);
				return seq(index);
			}
		case 2:
			{
				auto seq = lotus::sequences::hammersley<f32>::create();
				return seq(param, index)[0];
			}
		case 3:
			{
				auto seq = lotus::sequences::hammersley<f32>::create();
				return seq(param, index)[1];
			}
		}
		return 0.0f;
	};
	void update_taa_samples() {
		taa_samples.resize(taa_sample_count, zero);
		for (u32 i = 0; i < taa_sample_count; ++i) {
			taa_samples[i] = cvec2f32(
				get_taa_sample(taa_sequence_x, i + taa_sample_offset, taa_sample_param_x),
				get_taa_sample(taa_sequence_y, i + taa_sample_offset, taa_sample_param_y)
			);
		}
	};

	void fill_buffer(lren::structured_buffer_view buf, u32 value, lren::constant_uploader &uploader, std::u8string_view description) {
		buf = buf.view_as<u32>();
		shader_types::fill_buffer_constants data;
		data.size = buf.get_num_elements();
		data.value = value;
		_graphics_queue.run_compute_shader_with_thread_dimensions(
			fill_buffer_cs, cvec3u32(data.size, 1u, 1u),
			lren::all_resource_bindings(
				{
					{ 0, {
						{ 0, buf.bind_as_read_write() },
						{ 1, uploader.upload(data) },
					} },
				},
				{}
			),
			description
		);
	};
	void fill_texture3d(lren::image3d_view img, cvec4f32 value, lren::constant_uploader &uploader, std::u8string_view description) {
		shader_types::fill_texture3d_constants data;
		data.value = value;
		data.size = img.get_size();
		_graphics_queue.run_compute_shader_with_thread_dimensions(
			fill_texture3d_cs, img.get_size(),
			lren::all_resource_bindings(
				{
					{ 0, {
						{ 0, uploader.upload(data) },
						{ 1, img.bind_as_read_write() },
					} },
				},
				{}
			),
			description
		);
	};

	void resize_probe_buffers() {
		u32 num_probes = probe_density[0] * probe_density[1] * probe_density[2];

		u32 num_direct_reservoirs = num_probes * direct_reservoirs_per_probe;
		direct_reservoirs = _context->request_structured_buffer<shader_types::direct_lighting_reservoir>(
			u8"Direct Lighting Reservoirs", num_direct_reservoirs,
			lgpu::buffer_usage_mask::shader_read | lgpu::buffer_usage_mask::shader_write,
			runtime_buf_pool
		);
		u32 num_indirect_reservoirs = num_probes * indirect_reservoirs_per_probe;
		indirect_reservoirs = _context->request_structured_buffer<shader_types::indirect_lighting_reservoir>(
			u8"Indirect Lighting Reservoirs", num_indirect_reservoirs,
			lgpu::buffer_usage_mask::shader_read | lgpu::buffer_usage_mask::shader_write,
			runtime_buf_pool
		);
		probe_sh0 = _context->request_image3d(
			u8"Probe SH0", probe_density, 1, lgpu::format::r16g16b16a16_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);
		probe_sh1 = _context->request_image3d(
			u8"Probe SH1", probe_density, 1, lgpu::format::r16g16b16a16_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);
		probe_sh2 = _context->request_image3d(
			u8"Probe SH2", probe_density, 1, lgpu::format::r16g16b16a16_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);
		probe_sh3 = _context->request_image3d(
			u8"Probe SH3", probe_density, 1, lgpu::format::r16g16b16a16_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);

		clear_reservoirs = true;

		// compute transformation matrices
		cvec3f32 grid_size = probe_bounds.signed_size();
		mat33f32 rotscale = mat33f32::diagonal(grid_size).inverse();
		mat44f32 world_to_grid = mat44f32::identity();
		world_to_grid.set_block(0, 0, rotscale);
		world_to_grid.set_block(0, 3, rotscale * -probe_bounds.min);

		probe_constants.world_to_grid = world_to_grid;
		probe_constants.grid_to_world = world_to_grid.inverse();
		probe_constants.grid_size = probe_density;
		probe_constants.direct_reservoirs_per_probe = direct_reservoirs_per_probe;
		probe_constants.indirect_reservoirs_per_probe = indirect_reservoirs_per_probe;
	};

protected:
	static constexpr lgpu::queue_family _queues[] = { lgpu::queue_family::graphics, lgpu::queue_family::copy };

	lren::context::queue _graphics_queue = nullptr;

	std::unique_ptr<lren::debug_renderer> _debug_renderer; ///< Debug renderer.

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
	std::vector<std::filesystem::path> _get_additional_shader_include_paths() const override {
		return {
			_assets->asset_library_path / "shaders",
			std::getenv("LOTUS_RENDERER_TEST_SHADER_INCLUDE_PATH"),
		};
	}

	void _on_initialized() override {
		_graphics_queue = _context->get_queue(0);

		_debug_renderer = std::unique_ptr<lren::debug_renderer>(
			new auto(lren::debug_renderer::create(*_assets, _graphics_queue))
		);

		scene = std::make_unique<scene_representation>(*_assets, _graphics_queue);
		for (int i = 1; i < _argc; ++i) {
			if (_argv[i][0] != '-') {
				scene->load(_argv[i]);
			}
		}
		scene->finish_loading();

		runtime_tex_pool = _context->request_pool(u8"Run-time Textures");
		runtime_buf_pool = _context->request_pool(u8"Run-time Buffers");

		fs_quad_vs           = _assets->compile_shader_in_filesystem(_assets->asset_library_path / "shaders/misc/fullscreen_quad_vs.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs");
		blit_ps              = _assets->compile_shader_in_filesystem(_assets->asset_library_path / "shaders/misc/blit_ps.hlsl",            lgpu::shader_stage::pixel_shader, u8"main_ps");
		fill_buffer_cs       = _assets->compile_shader_in_filesystem("src/shaders/fill_buffer.hlsl",           lgpu::shader_stage::compute_shader, u8"main_cs");
		fill_texture3d_cs    = _assets->compile_shader_in_filesystem("src/shaders/fill_texture3d.hlsl",        lgpu::shader_stage::compute_shader, u8"main_cs");
		show_gbuffer_ps      = _assets->compile_shader_in_filesystem("src/shaders/gbuffer_visualization.hlsl", lgpu::shader_stage::pixel_shader,   u8"main_ps");
		visualize_probes_vs  = _assets->compile_shader_in_filesystem("src/shaders/visualize_probes.hlsl",      lgpu::shader_stage::vertex_shader,  u8"main_vs");
		visualize_probes_ps  = _assets->compile_shader_in_filesystem("src/shaders/visualize_probes.hlsl",      lgpu::shader_stage::pixel_shader,   u8"main_ps");
		shade_point_debug_cs = _assets->compile_shader_in_filesystem("src/shaders/shade_point_debug.hlsl",     lgpu::shader_stage::compute_shader, u8"main_cs");

		direct_update_cs          = _assets->compile_shader_in_filesystem("src/shaders/direct_reservoirs.hlsl",      lgpu::shader_stage::compute_shader, u8"main_cs");
		indirect_update_cs        = _assets->compile_shader_in_filesystem("src/shaders/indirect_reservoirs.hlsl",    lgpu::shader_stage::compute_shader, u8"main_cs");
		summarize_probes_cs       = _assets->compile_shader_in_filesystem("src/shaders/summarize_probes.hlsl",       lgpu::shader_stage::compute_shader, u8"main_cs");
		indirect_spatial_reuse_cs = _assets->compile_shader_in_filesystem("src/shaders/indirect_spatial_reuse.hlsl", lgpu::shader_stage::compute_shader, u8"main_cs");
		indirect_specular_cs      = _assets->compile_shader_in_filesystem("src/shaders/indirect_specular.hlsl",      lgpu::shader_stage::compute_shader, u8"main_cs");
		indirect_specular_vndf_cs = _assets->compile_shader_in_filesystem("src/shaders/indirect_specular.hlsl",      lgpu::shader_stage::compute_shader, u8"main_cs", { { u8"SAMPLE_VISIBLE_NORMALS", u8""} });
		lighting_cs               = _assets->compile_shader_in_filesystem("src/shaders/lighting.hlsl",               lgpu::shader_stage::compute_shader, u8"main_cs");
		sky_vs                    = _assets->compile_shader_in_filesystem(_assets->asset_library_path / "shaders/misc/fullscreen_quad_vs.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs", { { u8"FULLSCREEN_QUAD_DEPTH", u8"0.0" } });
		sky_ps                    = _assets->compile_shader_in_filesystem("src/shaders/sky.hlsl",                    lgpu::shader_stage::pixel_shader,   u8"main_ps");
		taa_cs                    = _assets->compile_shader_in_filesystem("src/shaders/taa.hlsl",                    lgpu::shader_stage::compute_shader, u8"main_cs");
		lighting_blit_ps          = _assets->compile_shader_in_filesystem("src/shaders/lighting_blit.hlsl",          lgpu::shader_stage::pixel_shader,   u8"main_ps");

		envmap_lut = _assets->get_image2d(lren::assets::identifier(_assets->asset_library_path / "envmap_lut.dds"), runtime_tex_pool);
		sky_hdri = _assets->get_null_image();

		cam_params = lotus::camera_parameters<f32>::create_look_at(zero, cvec3f32(100.0f, 100.0f, 100.0f));
		cam_control = lotus::camera_control<f32>(cam_params);

		update_taa_samples();
		resize_probe_buffers();

		rng = std::default_random_engine(std::random_device{}());
	}

	void _on_resize(lsys::window_events::resize &resize) override {
		cvec2u32 sz = resize.new_size;
		cam_params.aspect_ratio = sz[0] / static_cast<f32>(sz[1]);
		path_tracer_accum = _context->request_image2d(u8"Path Tracer Accumulation Buffer", sz, 1, lgpu::format::r32g32b32a32_float, lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write, runtime_tex_pool);
	}

	void _on_mouse_move(lsys::window_events::mouse::move &move) override {
		if (cam_control.on_mouse_move(move.new_position)) {
			num_accumulated_frames = 0;
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

	void _process_frame(lren::constant_uploader &uploader, lren::dependency constants_dep, lren::dependency asset_dep) override {
		_graphics_queue.acquire_dependency(constants_dep, u8"Wait for assets");
		if (asset_dep) {
			_graphics_queue.acquire_dependency(asset_dep, u8"Wait for constants");
		}

		if (clear_reservoirs) {
			fill_buffer(direct_reservoirs, 0, uploader, u8"Clear Direct Reservoir Buffer");
			fill_buffer(indirect_reservoirs, 0, uploader, u8"Clear Indirect Reservoir Buffer");
			fill_texture3d(probe_sh0, zero, uploader, u8"Clear Probe SH0");
			fill_texture3d(probe_sh1, zero, uploader, u8"Clear Probe SH1");
			fill_texture3d(probe_sh2, zero, uploader, u8"Clear Probe SH2");
			fill_texture3d(probe_sh3, zero, uploader, u8"Clear Probe SH3");
			clear_reservoirs = false;
		}

		{
			auto frame_tmr = _graphics_queue.start_timer(u8"Frame");

			const cvec2u32 window_size = _get_window_size();

			cvec2f32 taa_jitter = taa_samples[std::min(taa_phase, static_cast<u32>(taa_samples.size()))] - cvec2f32::filled(0.5f);
			auto cam = cam_params.into_camera(lotus::matm::divide(taa_jitter, (2u * window_size).into<f32>()));

			auto g_buf = lren::g_buffer::view::create(*_context, window_size, runtime_tex_pool);
			{ // g-buffer
				auto tmr = _graphics_queue.start_timer(u8"G-Buffer");

				lren::shader_types::view_data view_data;
				view_data.view                     = cam.view_matrix;
				view_data.projection               = cam.projection_matrix;
				view_data.jitter                   = cam.jitter_matrix;
				view_data.projection_view          = cam.projection_view_matrix;
				view_data.jittered_projection_view = cam.jittered_projection_view_matrix;
				view_data.prev_projection_view     = prev_cam.projection_view_matrix;
				view_data.rcp_viewport_size        = lotus::matm::reciprocal(window_size.into<f32>());

				auto pass = g_buf.begin_pass(_graphics_queue);
				lren::g_buffer::render_instances(
					pass, uploader,
					scene->instances, scene->gbuffer_instance_render_details, uploader.upload(view_data)
				);
				pass.end();
			}

			auto light_diffuse = _context->request_image2d(
				u8"Lighting Diffuse", window_size, 1, lgpu::format::r16g16b16a16_float,
				lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write | lgpu::image_usage_mask::color_render_target,
				runtime_tex_pool
			);
			auto light_specular = _context->request_image2d(
				u8"Lighting Specular", window_size, 1, lgpu::format::r16g16b16a16_float,
				lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
				runtime_tex_pool
			);

			shader_types::lighting_constants lighting_constants;
			lighting_constants.jittered_projection_view         = cam.jittered_projection_view_matrix;
			lighting_constants.inverse_jittered_projection_view = cam.inverse_jittered_projection_view_matrix;
			lighting_constants.camera                           = cvec4f32(cam_params.position, 1.0f);
			lighting_constants.depth_linearization_constants    = cam.depth_linearization_constants;
			lighting_constants.screen_size                      = window_size.into<u32>();
			lighting_constants.num_lights                       = static_cast<u32>(scene->lights.size());
			lighting_constants.trace_shadow_rays_for_naive      = trace_shadow_rays_naive;
			lighting_constants.trace_shadow_rays_for_reservoir  = trace_shadow_rays_reservoir;
			lighting_constants.lighting_mode                    = lighting_mode;
			lighting_constants.direct_diffuse_multiplier        = diffuse_mul;
			lighting_constants.direct_specular_multiplier       = specular_mul;
			lighting_constants.use_indirect                     = use_indirect_diffuse;
			lighting_constants.sky_scale                        = sky_scale;
			{
				cvec2f32 envmaplut_size = envmap_lut->image.get_size().into<f32>();
				cvec2f32 rcp_size = lotus::matm::reciprocal(envmaplut_size);
				lighting_constants.envmaplut_uvscale = lotus::matm::multiply(envmaplut_size - cvec2f32::filled(1.0f), rcp_size);
				lighting_constants.envmaplut_uvbias  = lotus::matm::multiply(cvec2f32::filled(0.5f), rcp_size);
			}

			u32 num_probes = probe_density[0] * probe_density[1] * probe_density[2];
			u32 num_indirect_reservoirs = num_probes * indirect_reservoirs_per_probe;
			auto spatial_indirect_reservoirs1 = _context->request_structured_buffer<shader_types::indirect_lighting_reservoir>(
				u8"Indirect Lighting Reservoirs", num_indirect_reservoirs,
				lgpu::buffer_usage_mask::shader_read | lgpu::buffer_usage_mask::shader_write,
				runtime_buf_pool
			);
			auto spatial_indirect_reservoirs2 = _context->request_structured_buffer<shader_types::indirect_lighting_reservoir>(
				u8"Indirect Lighting Reservoirs", num_indirect_reservoirs,
				lgpu::buffer_usage_mask::shader_read | lgpu::buffer_usage_mask::shader_write,
				runtime_buf_pool
			);

			if (update_probes || update_probes_this_frame) {
				update_probes_this_frame = false;

				{ // direct probes
					auto tmr = _graphics_queue.start_timer(u8"Update Direct Probes");

					shader_types::direct_reservoir_update_constants direct_update_constants;
					direct_update_constants.num_lights       = static_cast<u32>(scene->lights.size());
					direct_update_constants.sample_count_cap = direct_sample_count_cap;
					direct_update_constants.frame_index      = frame_index;

					lren::all_resource_bindings resources(
						{},
						{
							{ u8"probe_consts",      uploader.upload(probe_constants) },
							{ u8"constants",         uploader.upload(direct_update_constants) },
							{ u8"direct_reservoirs", direct_reservoirs.bind_as_read_write() },
							{ u8"all_lights",        scene->lights_buffer.bind_as_read_only() },
							{ u8"rtas",              scene->tlas },
						}
					);
					_graphics_queue.run_compute_shader_with_thread_dimensions(
						direct_update_cs, probe_density, std::move(resources), u8"Update Direct Probes"
					);
				}

				{ // indirect probes
					auto tmr = _graphics_queue.start_timer(u8"Update Indirect Probes");

					shader_types::indirect_reservoir_update_constants indirect_update_constants;
					indirect_update_constants.frame_index      = frame_index;
					indirect_update_constants.sample_count_cap = indirect_sample_count_cap;
					indirect_update_constants.sky_scale        = sky_scale;
					indirect_update_constants.temporal_reuse   = indirect_temporal_reuse;

					lren::all_resource_bindings resources(
						{
							{ 8, _assets->get_samplers() },
						},
						{
							{ u8"probe_consts",    uploader.upload(probe_constants) },
							{ u8"constants",       uploader.upload(indirect_update_constants) },
							{ u8"lighting_consts", uploader.upload(lighting_constants) },
							{ u8"direct_probes",   direct_reservoirs.bind_as_read_only() },
							{ u8"indirect_sh0",    probe_sh0.bind_as_read_only() },
							{ u8"indirect_sh1",    probe_sh1.bind_as_read_only() },
							{ u8"indirect_sh2",    probe_sh2.bind_as_read_only() },
							{ u8"indirect_sh3",    probe_sh3.bind_as_read_only() },
							{ u8"indirect_probes", indirect_reservoirs.bind_as_read_write() },
							{ u8"rtas",            scene->tlas },
							{ u8"sky_latlong",     sky_hdri->image.bind_as_read_only() },
							{ u8"envmap_lut",      envmap_lut->image.bind_as_read_only() },
							{ u8"textures",        _assets->get_images() },
							{ u8"positions",       scene->vertex_buffers },
							{ u8"normals",         scene->normal_buffers },
							{ u8"tangents",        scene->tangent_buffers },
							{ u8"uvs",             scene->uv_buffers },
							{ u8"indices",         scene->index_buffers },
							{ u8"instances",       scene->instances_buffer.bind_as_read_only() },
							{ u8"geometries",      scene->geometries_buffer.bind_as_read_only() },
							{ u8"materials",       scene->materials_buffer.bind_as_read_only() },
							{ u8"all_lights",      scene->lights_buffer.bind_as_read_only() },
						}
					);
					_graphics_queue.run_compute_shader_with_thread_dimensions(
						indirect_update_cs, probe_density, std::move(resources), u8"Update Indirect Probes"
					);
				}

				if (indirect_spatial_reuse) { // indirect spatial reuse
					auto tmr = _graphics_queue.start_timer(u8"Indirect Spatial Reuse");

					cvec3i offsets[6] = {
						{  1,  0,  0 },
						{ -1,  0,  0 },
						{  0,  1,  0 },
						{  0, -1,  0 },
						{  0,  0,  1 },
						{  0,  0, -1 },
					};

					for (u32 i = 0; i < 3; ++i) {
						shader_types::indirect_spatial_reuse_constants reuse_constants;
						reuse_constants.offset               = offsets[i * 2 + rng() % 2];
						reuse_constants.frame_index          = frame_index;
						reuse_constants.visibility_test_mode = indirect_spatial_reuse_visibility_test_mode;

						lren::all_resource_bindings resources(
							{},
							{
								{ u8"rtas",              scene->tlas },
								{ u8"input_reservoirs",  (i == 0 ? indirect_reservoirs : spatial_indirect_reservoirs1).bind_as_read_only() },
								{ u8"output_reservoirs", spatial_indirect_reservoirs2.bind_as_read_write() },
								{ u8"probe_consts",      uploader.upload(probe_constants) },
								{ u8"constants",         uploader.upload(reuse_constants) },
							}
						);
						_graphics_queue.run_compute_shader_with_thread_dimensions(indirect_spatial_reuse_cs, probe_density, std::move(resources), u8"Spatial Indirect Reuse");
						std::swap(spatial_indirect_reservoirs1, spatial_indirect_reservoirs2);
					}
				} else {
					spatial_indirect_reservoirs1 = indirect_reservoirs;
				}

				{ // summarize probes
					auto tmr = _graphics_queue.start_timer(u8"Summarize Probes");

					shader_types::summarize_probe_constants constants;
					constants.ra_alpha = sh_ra_factor;

					lren::all_resource_bindings resources(
						{},
						{
							{ u8"indirect_reservoirs", spatial_indirect_reservoirs1.bind_as_read_only() },
							{ u8"probe_sh0",           probe_sh0.bind_as_read_write() },
							{ u8"probe_sh1",           probe_sh1.bind_as_read_write() },
							{ u8"probe_sh2",           probe_sh2.bind_as_read_write() },
							{ u8"probe_sh3",           probe_sh3.bind_as_read_write() },
							{ u8"probe_consts",        uploader.upload(probe_constants) },
							{ u8"constants",           uploader.upload(constants) },
						}
					);
					_graphics_queue.run_compute_shader_with_thread_dimensions(
						summarize_probes_cs, probe_density, std::move(resources), u8"Summarize Probes"
					);
				}
			}

			{ // lighting
				auto tmr = _graphics_queue.start_timer(u8"Direct & Indirect Diffuse");

				lren::all_resource_bindings resources(
					{
						{ 1, _assets->get_samplers() },
					},
					{
						{ u8"gbuffer_albedo_glossiness", g_buf.albedo_glossiness.bind_as_read_only() },
						{ u8"gbuffer_normal",            g_buf.normal.bind_as_read_only() },
						{ u8"gbuffer_metalness",         g_buf.metalness.bind_as_read_only() },
						{ u8"gbuffer_depth",             g_buf.depth_stencil.bind_as_read_only() },
						{ u8"out_diffuse",               light_diffuse.bind_as_read_write() },
						{ u8"out_specular",              light_specular.bind_as_read_write() },
						{ u8"all_lights",                scene->lights_buffer.bind_as_read_only() },
						{ u8"direct_reservoirs",         direct_reservoirs.bind_as_read_only() },
						{ u8"indirect_sh0",              probe_sh0.bind_as_read_only() },
						{ u8"indirect_sh1",              probe_sh1.bind_as_read_only() },
						{ u8"indirect_sh2",              probe_sh2.bind_as_read_only() },
						{ u8"indirect_sh3",              probe_sh3.bind_as_read_only() },
						{ u8"rtas",                      scene->tlas },
						{ u8"constants",                 uploader.upload(lighting_constants) },
						{ u8"probe_consts",              uploader.upload(probe_constants) },
						{ u8"sky_latlong",               sky_hdri->image.bind_as_read_only() },
					}
				);
				_graphics_queue.run_compute_shader_with_thread_dimensions(
					lighting_cs, cvec3u32(window_size.into<u32>(), 1u),
					std::move(resources), u8"Lighting"
				);
			}

			{ // sky
				auto tmr = _graphics_queue.start_timer(u8"Sky");

				shader_types::sky_constants constants;
				{
					auto rot_only = mat44f32::identity();
					rot_only.set_block(0, 0, cam.view_matrix.block<3, 3>(0, 0));
					constants.inverse_projection_view_no_translation = (cam.projection_matrix * rot_only).inverse();

					auto prev_rot_only = mat44f32::identity();
					prev_rot_only.set_block(0, 0, prev_cam.view_matrix.block<3, 3>(0, 0));
					constants.prev_projection_view_no_translation = prev_cam.projection_matrix * prev_rot_only;
				}
				constants.znear     = cam_params.near_plane;
				constants.sky_scale = sky_scale;

				lren::all_resource_bindings resources(
					{
						{ 1, _assets->get_samplers() },
					},
					{
						{ u8"sky_latlong", sky_hdri->image.bind_as_read_only() },
						{ u8"constants",   uploader.upload(constants) },
					}
				);
				lren::graphics_pipeline_state pipeline(
					{
						lgpu::render_target_blend_options::disabled(),
						lgpu::render_target_blend_options::disabled(),
					},
					lgpu::rasterizer_options(lgpu::depth_bias_options::disabled(), lgpu::front_facing_mode::clockwise, lgpu::cull_mode::none, false),
					lgpu::depth_stencil_options(true, false, lgpu::comparison_function::equal, false, 0, 0, lgpu::stencil_options::always_pass_no_op(), lgpu::stencil_options::always_pass_no_op())
				);

				auto pass = _graphics_queue.begin_pass(
					{
						lren::image2d_color(light_diffuse, lgpu::color_render_target_access::create_preserve_and_write()),
						lren::image2d_color(g_buf.velocity, lgpu::color_render_target_access::create_preserve_and_write()),
					},
					lren::image2d_depth_stencil(g_buf.depth_stencil, lgpu::depth_render_target_access::create_preserve_and_write(), lgpu::stencil_render_target_access::create_discard()),
					window_size, u8"Sky"
				);
				pass.draw_instanced({}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list, std::move(resources), sky_vs, sky_ps, std::move(pipeline), 1, u8"Sky");
				pass.end();
			}

			auto indirect_specular = _context->request_image2d(u8"Indirect Specular", window_size, 1, lgpu::format::r32g32b32a32_float, lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write, runtime_tex_pool);

			{ // indirect specular
				auto tmr = _graphics_queue.start_timer(u8"Indirect Specular");

				shader_types::indirect_specular_constants constants;
				constants.enable_mis                        = enable_indirect_specular_mis;
				constants.use_screenspace_samples           = use_ss_indirect_specular;
				constants.frame_index                       = frame_index;
				constants.approx_indirect_indirect_specular = approx_indirect_indirect_specular;
				constants.use_approx_for_everything         = debug_approx_for_indirect;
				lren::all_resource_bindings resources(
					{
						{ 8, _assets->get_samplers() },
					},
					{
						{ u8"probe_consts",              uploader.upload(probe_constants) },
						{ u8"constants",                 uploader.upload(constants) },
						{ u8"lighting_consts",           uploader.upload(lighting_constants) },
						{ u8"direct_probes",             direct_reservoirs.bind_as_read_only() },
						{ u8"indirect_probes",           spatial_indirect_reservoirs1.bind_as_read_only() },
						{ u8"indirect_sh0",              probe_sh0.bind_as_read_only() },
						{ u8"indirect_sh1",              probe_sh1.bind_as_read_only() },
						{ u8"indirect_sh2",              probe_sh2.bind_as_read_only() },
						{ u8"indirect_sh3",              probe_sh3.bind_as_read_only() },
						{ u8"diffuse_lighting",          light_diffuse.bind_as_read_only() },
						{ u8"envmap_lut",                envmap_lut->image.bind_as_read_only() },
						{ u8"out_specular",              indirect_specular.bind_as_read_write() },
						{ u8"rtas",                      scene->tlas },
						{ u8"gbuffer_albedo_glossiness", g_buf.albedo_glossiness.bind_as_read_only() },
						{ u8"gbuffer_normal",            g_buf.normal.bind_as_read_only() },
						{ u8"gbuffer_metalness",         g_buf.metalness.bind_as_read_only() },
						{ u8"gbuffer_depth",             g_buf.depth_stencil.bind_as_read_only() },
						{ u8"sky_latlong",               sky_hdri->image.bind_as_read_only() },
						{ u8"textures",                  _assets->get_images() },
						{ u8"positions",                 scene->vertex_buffers },
						{ u8"normals",                   scene->normal_buffers },
						{ u8"tangents",                  scene->tangent_buffers },
						{ u8"uvs",                       scene->uv_buffers },
						{ u8"indices",                   scene->index_buffers },
						{ u8"instances",                 scene->instances_buffer.bind_as_read_only() },
						{ u8"geometries",                scene->geometries_buffer.bind_as_read_only() },
						{ u8"materials",                 scene->materials_buffer.bind_as_read_only() },
						{ u8"all_lights",                scene->lights_buffer.bind_as_read_only() },
					}
				);

				_graphics_queue.run_compute_shader_with_thread_dimensions(
					indirect_specular_use_visible_normals ? indirect_specular_vndf_cs : indirect_specular_cs,
					cvec3u32(window_size.into<u32>(), 1u),
					std::move(resources), u8"Indirect Specular"
				);
			}

			if (shade_point_debug_mode != 0) {
				f32 tan_half_fovy = std::tan(0.5f * cam_params.fov_y_radians);
				cvec3f32 half_right = cam.unit_right * cam_params.aspect_ratio * tan_half_fovy;
				cvec3f32 half_down = cam.unit_up * -tan_half_fovy;
				cvec3f32 pixel_x = half_right / (0.5f * window_size[0]);
				cvec3f32 pixel_y = half_down / (0.5f * window_size[1]);

				shader_types::shade_point_debug_constants constants;
				constants.camera      = cvec4f32(cam_params.position, 1.0f);
				constants.x           = cvec4f32(pixel_x, 0.0f);
				constants.y           = cvec4f32(pixel_y, 0.0f);
				constants.top_left    = cvec4f32(cam.unit_forward - half_right - half_down, 0.0f);
				constants.window_size = window_size.into<u32>();
				constants.num_lights  = static_cast<u32>(scene->lights.size());
				constants.mode        = shade_point_debug_mode;
				constants.num_frames  = ++num_accumulated_frames;

				lren::all_resource_bindings resources(
					{
						{ 8, _assets->get_samplers() },
					},
					{
						{ u8"probe_consts",    uploader.upload(probe_constants) },
						{ u8"constants",       uploader.upload(constants) },
						{ u8"lighting_consts", uploader.upload(lighting_constants) },
						{ u8"direct_probes",   direct_reservoirs.bind_as_read_only() },
						{ u8"indirect_sh0",    probe_sh0.bind_as_read_only() },
						{ u8"indirect_sh1",    probe_sh1.bind_as_read_only() },
						{ u8"indirect_sh2",    probe_sh2.bind_as_read_only() },
						{ u8"indirect_sh3",    probe_sh3.bind_as_read_only() },
						{ u8"envmap_lut",      envmap_lut->image.bind_as_read_only() },
						{ u8"out_irradiance",  light_diffuse.bind_as_read_write() },
						{ u8"out_accum",       path_tracer_accum.bind_as_read_write() },
						{ u8"rtas",            scene->tlas },
						{ u8"textures",        _assets->get_images() },
						{ u8"positions",       scene->vertex_buffers },
						{ u8"normals",         scene->normal_buffers },
						{ u8"tangents",        scene->tangent_buffers },
						{ u8"uvs",             scene->uv_buffers },
						{ u8"indices",         scene->index_buffers },
						{ u8"instances",       scene->instances_buffer.bind_as_read_only() },
						{ u8"geometries",      scene->geometries_buffer.bind_as_read_only() },
						{ u8"materials",       scene->materials_buffer.bind_as_read_only() },
						{ u8"all_lights",      scene->lights_buffer.bind_as_read_only() },
					}
				);
				_graphics_queue.run_compute_shader_with_thread_dimensions(
					shade_point_debug_cs, cvec3u32(window_size.into<u32>(), 1u),
					std::move(resources), u8"Shade Point Debug"
				);
			}

			auto irradiance = _context->request_image2d(u8"Previous Irradiance", window_size, 1, lgpu::format::r16g16b16a16_float, lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write, runtime_tex_pool);

			{ // TAA
				auto tmr = _graphics_queue.start_timer(u8"TAA");

				lren::graphics_pipeline_state state(
					{ lgpu::render_target_blend_options::disabled() },
					nullptr,
					nullptr
				);
				shader_types::taa_constants constants;
				constants.viewport_size         = window_size.into<u32>();
				constants.rcp_viewport_size     = lotus::matm::reciprocal(window_size.into<f32>());
				constants.use_indirect_specular = use_indirect_specular;
				constants.ra_factor             = taa_ra_factor;
				constants.enable_taa            = enable_taa && prev_irradiance.is_valid();
				lren::all_resource_bindings resources(
					{
						{ 1, _assets->get_samplers() },
					},
					{
						{ u8"diffuse_lighting",  light_diffuse.bind_as_read_only() },
						{ u8"specular_lighting", light_specular.bind_as_read_only() },
						{ u8"indirect_specular", indirect_specular.bind_as_read_only() },
						{ u8"prev_irradiance",   prev_irradiance ? prev_irradiance.bind_as_read_only() : _assets->get_invalid_image()->image.bind_as_read_only() },
						{ u8"motion_vectors",    g_buf.velocity.bind_as_read_only() },
						{ u8"out_irradiance",    irradiance.bind_as_read_write() },
						{ u8"constants",         uploader.upload(constants) },
					}
				);

				_graphics_queue.run_compute_shader_with_thread_dimensions(
					taa_cs, cvec3u32(window_size.into<u32>(), 1u), std::move(resources), u8"TAA"
				);

				prev_irradiance = irradiance;
			}

			{ // lighting blit
				shader_types::lighting_blit_constants constants;
				constants.lighting_scale = lighting_scale;
				lren::all_resource_bindings resources(
					{},
					{
						{ u8"constants",  uploader.upload(constants) },
						{ u8"irradiance", (shade_point_debug_mode != 0 ? light_diffuse : irradiance).bind_as_read_only() },
					}
				);

				lren::graphics_pipeline_state pipeline(
					{ lgpu::render_target_blend_options::disabled() },
					lgpu::rasterizer_options(lgpu::depth_bias_options::disabled(), lgpu::front_facing_mode::clockwise, lgpu::cull_mode::none, false),
					lgpu::depth_stencil_options(false, false, lgpu::comparison_function::always, false, 0, 0, lgpu::stencil_options::always_pass_no_op(), lgpu::stencil_options::always_pass_no_op())
				);

				auto pass = _graphics_queue.begin_pass(
					{ lren::image2d_color(_swap_chain, lgpu::color_render_target_access::create_discard_then_write()) },
					nullptr,
					window_size, u8"Lighting Blit"
				);
				pass.draw_instanced({}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list, std::move(resources), fs_quad_vs, lighting_blit_ps, std::move(pipeline), 1, u8"Lighting Blit");
				pass.end();
			}

			{ // debug views
				if (gbuffer_visualization > 0) {
					lren::graphics_pipeline_state state(
						{ lgpu::render_target_blend_options::disabled() },
						nullptr,
						nullptr
					);
					shader_types::gbuffer_visualization_constants constants;
					constants.mode = gbuffer_visualization;
					constants.exclude_sky = true;
					lren::all_resource_bindings resources(
						{
							{ 1, _assets->get_samplers() },
						},
						{
							{ u8"gbuffer_albedo_glossiness", g_buf.albedo_glossiness.bind_as_read_only() },
							{ u8"gbuffer_normal",            g_buf.normal.bind_as_read_only() },
							{ u8"gbuffer_metalness",         g_buf.metalness.bind_as_read_only() },
							{ u8"gbuffer_depth",             g_buf.depth_stencil.bind_as_read_only() },
							{ u8"constants",                 uploader.upload(constants) },
						}
					);

					auto pass = _graphics_queue.begin_pass(
						{ lren::image2d_color(_swap_chain, lgpu::color_render_target_access::create_discard_then_write()) },
						nullptr, window_size, u8"GBuffer Visualization Pass"
					);
					pass.draw_instanced(
						{}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list,
						std::move(resources), fs_quad_vs, show_gbuffer_ps, std::move(state), 1, u8"GBuffer Visualization");
					pass.end();
				}

				if (visualize_probes_mode != 0) {
					lren::graphics_pipeline_state state(
						{ lgpu::render_target_blend_options::disabled() },
						nullptr,
						lgpu::depth_stencil_options(
							true, true, lgpu::comparison_function::greater,
							false, 0, 0, lgpu::stencil_options::always_pass_no_op(), lgpu::stencil_options::always_pass_no_op()
						)
					);

					shader_types::visualize_probes_constants constants;
					constants.projection_view = cam.projection_view_matrix;
					constants.unit_right      = cam.unit_right;
					constants.size            = visualize_probe_size;
					constants.unit_down       = cam.unit_up;
					constants.mode            = visualize_probes_mode;
					constants.unit_forward    = cam.unit_forward;
					constants.lighting_scale  = lighting_scale;

					lren::all_resource_bindings resources(
						{},
						{
							{ u8"probe_consts", uploader.upload(probe_constants) },
							{ u8"constants",    uploader.upload(constants) },
							{ u8"probe_sh0",    probe_sh0.bind_as_read_only() },
							{ u8"probe_sh1",    probe_sh1.bind_as_read_only() },
							{ u8"probe_sh2",    probe_sh2.bind_as_read_only() },
							{ u8"probe_sh3",    probe_sh3.bind_as_read_only() },
						}
					);

					u32 num_probes = probe_density[0] * probe_density[1] * probe_density[2];

					auto pass = _graphics_queue.begin_pass(
						{ lren::image2d_color(_swap_chain, lgpu::color_render_target_access::create_preserve_and_write()) },
						lren::image2d_depth_stencil(g_buf.depth_stencil, lgpu::depth_render_target_access::create_preserve_and_write()),
						window_size, u8"Probe Visualization Pass"
					);
					pass.draw_instanced(
						{}, 6, nullptr, 0, lgpu::primitive_topology::triangle_list,
						std::move(resources), visualize_probes_vs, visualize_probes_ps, std::move(state),
						num_probes, u8"Probe Visualization"
					);
					pass.end();
				}

				for (const auto &l : scene->lights) {
					_debug_renderer->add_locator(l.position, lotus::linear_rgba_f32(1.0f, 0.0f, 0.0f, 1.0f));
				}

				// debug drawing
				_debug_renderer->flush(
					lren::image2d_color(_swap_chain, lgpu::color_render_target_access::create_preserve_and_write()),
					lren::image2d_depth_stencil(g_buf.depth_stencil, lgpu::depth_render_target_access::create_preserve_and_write()),
					_get_window_size(),
					cam.projection_view_matrix,
					uploader
				);
			}

			// update
			prev_cam = cam;
			++frame_index;
			taa_phase = (taa_phase + 1) % taa_samples.size();
		}
	}

	void _process_imgui() override {
		bool needs_resizing = false;

		if (ImGui::Begin("Controls")) {
			ImGui::SliderFloat("Lighting Scale", &lighting_scale, 0.01f, 100.0f, "%.02f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
			ImGui::InputText("Sky HDRI Path", sky_hdri_path, std::size(sky_hdri_path));
			if (ImGui::Button("Load HDRI")) {
				if (std::filesystem::exists(sky_hdri_path)) {
					sky_hdri = _assets->get_image2d(lren::assets::identifier(sky_hdri_path), scene->geom_texture_pool);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Unload HDRI")) {
				sky_hdri = _assets->get_null_image();
			}
			ImGui::SliderFloat("Sky Scale", &sky_scale, 0.01f, 10000.0f, "%.02f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
			ImGui::Combo("Show G-Buffer", &gbuffer_visualization, "Disabled\0Albedo\0Glossiness\0Normal\0Metalness\0Emissive\0");
			ImGui::Checkbox("Trace Naive Shadow Rays", &trace_shadow_rays_naive);
			ImGui::Checkbox("Trace Reservoir Shadow Rays", &trace_shadow_rays_reservoir);
			ImGui::Combo("Lighting Mode", &lighting_mode, "None\0Reservoir\0Naive\0");
			ImGui::SliderFloat("Direct Diffuse Multiplier", &diffuse_mul, 0.0f, 1.0f);
			ImGui::SliderFloat("Direct Specular Multiplier", &specular_mul, 0.0f, 1.0f);
			if (ImGui::Combo("Shade Point Debug Mode", &shade_point_debug_mode, "Off\0Lighting\0Albedo\0Normal\0Path Tracer\0")) {
				num_accumulated_frames = 0;
			}
			ImGui::Separator();

			ImGui::Checkbox("Enable TAA", &enable_taa);
			ImGui::SliderFloat("TAA RA Factor", &taa_ra_factor, 0.0f, 1.0f);
			{
				bool regen_sequence = false;
				const char *taa_sample_modes = "None\0Halton\0Hammersley X\0Hammersley Y\0";
				regen_sequence = ImGui::Combo("TAA Sequence X", &taa_sequence_x, taa_sample_modes) || regen_sequence;
				regen_sequence = ImGui::Combo("TAA Sequence Y", &taa_sequence_y, taa_sample_modes) || regen_sequence;
				regen_sequence = ImGui_SliderT<u32>("TAA Sequence Offset", &taa_sample_offset, 1, 512) || regen_sequence;
				regen_sequence = ImGui_SliderT<u32>("TAA Sequence X Param", &taa_sample_param_x, 1, 32) || regen_sequence;
				regen_sequence = ImGui_SliderT<u32>("TAA Sequence Y Param", &taa_sample_param_y, 1, 32) || regen_sequence;
				regen_sequence = ImGui_SliderT<u32>("TAA Sequence Length", &taa_sample_count, 1, 512, nullptr, ImGuiSliderFlags_Logarithmic) || regen_sequence;
				if (regen_sequence) {
					update_taa_samples();
				}
			}
			{ // draw samples
				constexpr cvec2f32 canvas_size(150.0f, 150.0f);
				constexpr f32 dot_radius = 2.0f;

				auto to_imvec2 = [](cvec2f32 p) {
					return ImVec2(p[0], p[1]);
				};
				auto to_cvec2f = [](ImVec2 p) {
					return cvec2f32(p.x, p.y);
				};

				auto *list = ImGui::GetWindowDrawList();
				auto pos = to_cvec2f(ImGui::GetCursorScreenPos());
				list->AddRectFilled(to_imvec2(pos), to_imvec2(pos + canvas_size), ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));

				for (usize i = 0; i < taa_samples.size(); ++i) {
					cvec2f32 smp = taa_samples[i];
					cvec2f32 dot_pos = pos + lotus::matm::multiply(smp, canvas_size);
					f32 seq_pos = i / static_cast<f32>(taa_samples.size() - 1);
					list->AddCircleFilled(to_imvec2(dot_pos), dot_radius, ImGui::ColorConvertFloat4ToU32(ImVec4(seq_pos, 0.0f, 1.0f - seq_pos, 1.0f)));
				}

				ImGui::SetCursorScreenPos(to_imvec2(pos + cvec2f32(0.0f, canvas_size[1])));
			}
			ImGui::Separator();

			ImGui::Combo("Visualize Probes", &visualize_probes_mode, "None\0Specular\0Diffuse\0Normal\0");
			ImGui::SliderFloat("Visualize Probes Size", &visualize_probe_size, 0.0f, 1.0f);
			{
				auto probes_int = probe_density.into<int>();
				int probes[3] = { probes_int[0], probes_int[1], probes_int[2] };
				needs_resizing = ImGui::SliderInt3("Num Probes", probes, 2, 100) || needs_resizing;
				probe_density = cvec3i(probes[0], probes[1], probes[2]).into<u32>();
			}
			needs_resizing = ImGui_SliderT<u32>("Direct Reservoirs Per Probe", &direct_reservoirs_per_probe, 1, 20) || needs_resizing;
			needs_resizing = ImGui_SliderT<u32>("Indirect Reservoirs Per Probe", &indirect_reservoirs_per_probe, 1, 20) || needs_resizing;
			{
				f32 rx[2] = { probe_bounds.min[0], probe_bounds.max[0] };
				f32 ry[2] = { probe_bounds.min[1], probe_bounds.max[1] };
				f32 rz[2] = { probe_bounds.min[2], probe_bounds.max[2] };
				needs_resizing = ImGui::SliderFloat2("Range X", rx, -20.0f, 20.0f) || needs_resizing;
				needs_resizing = ImGui::SliderFloat2("Range Y", ry, -20.0f, 20.0f) || needs_resizing;
				needs_resizing = ImGui::SliderFloat2("Range Z", rz, -20.0f, 20.0f) || needs_resizing;
				probe_bounds = lotus::aab3f32::create_from_min_max({ rx[0], ry[0], rz[0] }, { rx[1], ry[1], rz[1] });
			}
			ImGui::Separator();

			if (ImGui::Button("Reset Probes")) {
				needs_resizing = true;
			}
			ImGui::Checkbox("Update Probes", &update_probes);
			if (ImGui::Button("Update Probes This Frame")) {
				update_probes_this_frame = true;
			}
			ImGui::Checkbox("Show Indirect Diffuse", &use_indirect_diffuse);
			ImGui::Checkbox("Show Indirect Specular", &use_indirect_specular);
			ImGui::Checkbox("Indirect Specular: Sample Visible Normals", &indirect_specular_use_visible_normals);
			ImGui::Checkbox("Use Indirect Specular MIS", &enable_indirect_specular_mis);
			ImGui::Checkbox("Use Screen-space Samples For Indirect Specular", &use_ss_indirect_specular);
			ImGui::Checkbox("Approximate Indirect Indirect Specular", &approx_indirect_indirect_specular);
			ImGui::Checkbox("Debug Use Approximation For All Indirect Specular", &debug_approx_for_indirect);
			ImGui::Checkbox("Indirect Temporal Reuse", &indirect_temporal_reuse);
			ImGui::Checkbox("Indirect Spatial Reuse", &indirect_spatial_reuse);
			ImGui_SliderT<u32>("Indirect Spatial Reuse Passes", &indirect_spatial_reuse_passes, 1, 3);
			ImGui::Combo("Indirect Spatial Reuse Visibility Test Mode", &indirect_spatial_reuse_visibility_test_mode, "None\0Simple\0Full\0");
			ImGui::SliderFloat("SH RA Factor", &sh_ra_factor, 0.0f, 1.0f);
			ImGui_SliderT<u32>("Direct Sample Count Cap", &direct_sample_count_cap, 1, 10000, "%d", ImGuiSliderFlags_Logarithmic);
			ImGui_SliderT<u32>("Indirect Sample Count Cap", &indirect_sample_count_cap, 1, 10000, "%d", ImGuiSliderFlags_Logarithmic);
		}
		ImGui::End();
		if (needs_resizing) {
			resize_probe_buffers();
		}

		_show_statistics_window();
	}
};

int main(int argc, char **argv) {
	restir_probe_app app(argc, argv);
	app.initialize();
	return app.run();
}
