#include <cinttypes>
#include <queue>
#include <random>

#include <scene.h>
#include <camera_control.h>

#include <lotus/math/sequences.h>
#include <lotus/utils/camera.h>
#include <lotus/renderer/g_buffer.h>
#include <lotus/renderer/debug_drawing.h>

#include <lotus/renderer/shader_types_include_wrapper.h>
namespace shader_types {
#include "shaders/shader_types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>

#include <imgui.h>
#include <lotus/renderer/dear_imgui.h>
#include <lotus/system/dear_imgui.h>

template <typename T> struct ImGuiAutoDataType;
template <> struct ImGuiAutoDataType<std::uint32_t> {
	constexpr static ImGuiDataType value = ImGuiDataType_U32;
};
template <typename T> constexpr static ImGuiDataType ImGuiAutoDataType_v = ImGuiAutoDataType<T>::value;

template <typename T> static bool ImGui_SliderT(const char *label, T *data, T min, T max, const char *format = nullptr, ImGuiSliderFlags flags = 0) {
	return ImGui::SliderScalar(label, ImGuiAutoDataType_v<T>, data, &min, &max, format, flags);
}

struct sig_pair {
	sig_pair(lren::execution::constant_buffer_signature sig, std::uint32_t c) :
		signature(sig), count(c) {
	}

	[[nodiscard]] friend std::strong_ordering operator<=>(sig_pair lhs, sig_pair rhs) {
		return lhs.count <=> rhs.count;
	}

	lren::execution::constant_buffer_signature signature;
	std::uint32_t count;
};

int main(int argc, char **argv) {
	lsys::application app(u8"ReSTIR Probes");
	auto wnd = app.create_window();

	/*auto options = lgpu::context_options::enable_validation | lgpu::context_options::enable_debug_info;*/
	auto options = lgpu::context_options::none;
	auto gctx = lgpu::context::create(options);
	lgpu::device gdev = nullptr;
	std::vector<lgpu::command_queue> gqueues;
	lgpu::adapter_properties gprop = uninitialized;
	gctx.enumerate_adapters([&](lgpu::adapter adap) -> bool {
		gprop = adap.get_properties();
		log().debug<u8"Device: {}">(lstr::to_generic(gprop.name));
		if (gprop.is_discrete) {
			log().debug<u8"Selected">();
			auto &&[dev, queues] = adap.create_device({ lgpu::queue_type::graphics, lgpu::queue_type::compute });
			gdev = std::move(dev);
			gqueues = std::move(queues);
			return false;
		}
		return true;
	});
	auto gshu = lgpu::shader_utility::create();

	auto rctx = lren::context::create(gctx, gprop, gdev, gqueues[0], gqueues[1]);
	auto rassets = lren::assets::manager::create(rctx, &gshu);
	rassets.asset_library_path = "D:/Documents/Projects/lotus/lotus/renderer/include/lotus/renderer/assets";
	rassets.additional_shader_includes = {
		rassets.asset_library_path / "shaders",
		"D:/Documents/Projects/lotus/test/renderer/common/include",
	};

	lren::execution::batch_statistics_early batch_stats_early = zero;
	lren::execution::batch_statistics_late batch_stats_late = zero;
	rctx.on_batch_statistics_available = [&](std::uint32_t, lren::execution::batch_statistics_late stats) {
		batch_stats_late = std::move(stats);
	};

	scene_representation scene(rassets);
	for (int i = 1; i < argc; ++i) {
		scene.load(argv[i]);
	}
	scene.finish_loading();

	auto debug_render = lren::debug_renderer::create(rassets);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	auto imgui_rctx = lren::dear_imgui::context::create(rassets);
	auto imgui_sctx = lsys::dear_imgui::context::create();

	cvec2u32 window_size = zero;
	std::uint32_t frame_index = 0;

	auto runtime_tex_pool = rctx.request_pool(u8"Run-time Textures");
	auto runtime_buf_pool = rctx.request_pool(u8"Run-time Buffers");

	auto swapchain = rctx.request_swap_chain(u8"Main Swap Chain", wnd, 2, { lgpu::format::r8g8b8a8_srgb, lgpu::format::b8g8r8a8_srgb });

	auto fs_quad_vs           = rassets.compile_shader_in_filesystem(rassets.asset_library_path / "shaders/misc/fullscreen_quad_vs.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs");
	auto blit_ps              = rassets.compile_shader_in_filesystem(rassets.asset_library_path / "shaders/misc/blit_ps.hlsl",            lgpu::shader_stage::pixel_shader, u8"main_ps");
	auto fill_buffer_cs       = rassets.compile_shader_in_filesystem("src/shaders/fill_buffer.hlsl",           lgpu::shader_stage::compute_shader, u8"main_cs");
	auto fill_texture3d_cs    = rassets.compile_shader_in_filesystem("src/shaders/fill_texture3d.hlsl",        lgpu::shader_stage::compute_shader, u8"main_cs");
	auto show_gbuffer_ps      = rassets.compile_shader_in_filesystem("src/shaders/gbuffer_visualization.hlsl", lgpu::shader_stage::pixel_shader,   u8"main_ps");
	auto visualize_probes_vs  = rassets.compile_shader_in_filesystem("src/shaders/visualize_probes.hlsl",      lgpu::shader_stage::vertex_shader,  u8"main_vs");
	auto visualize_probes_ps  = rassets.compile_shader_in_filesystem("src/shaders/visualize_probes.hlsl",      lgpu::shader_stage::pixel_shader,   u8"main_ps");
	auto shade_point_debug_cs = rassets.compile_shader_in_filesystem("src/shaders/shade_point_debug.hlsl",     lgpu::shader_stage::compute_shader, u8"main_cs");

	auto direct_update_cs          = rassets.compile_shader_in_filesystem("src/shaders/direct_reservoirs.hlsl",      lgpu::shader_stage::compute_shader, u8"main_cs");
	auto indirect_update_cs        = rassets.compile_shader_in_filesystem("src/shaders/indirect_reservoirs.hlsl",    lgpu::shader_stage::compute_shader, u8"main_cs");
	auto summarize_probes_cs       = rassets.compile_shader_in_filesystem("src/shaders/summarize_probes.hlsl",       lgpu::shader_stage::compute_shader, u8"main_cs");
	auto indirect_spatial_reuse_cs = rassets.compile_shader_in_filesystem("src/shaders/indirect_spatial_reuse.hlsl", lgpu::shader_stage::compute_shader, u8"main_cs");
	auto indirect_specular_cs      = rassets.compile_shader_in_filesystem("src/shaders/indirect_specular.hlsl",      lgpu::shader_stage::compute_shader, u8"main_cs");
	auto indirect_specular_vndf_cs = rassets.compile_shader_in_filesystem("src/shaders/indirect_specular.hlsl",      lgpu::shader_stage::compute_shader, u8"main_cs", { { u8"SAMPLE_VISIBLE_NORMALS", u8""} });
	auto lighting_cs               = rassets.compile_shader_in_filesystem("src/shaders/lighting.hlsl",               lgpu::shader_stage::compute_shader, u8"main_cs");
	auto sky_vs                    = rassets.compile_shader_in_filesystem(rassets.asset_library_path / "shaders/misc/fullscreen_quad_vs.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs", { { u8"FULLSCREEN_QUAD_DEPTH", u8"0.0" } });
	auto sky_ps                    = rassets.compile_shader_in_filesystem("src/shaders/sky.hlsl",                    lgpu::shader_stage::pixel_shader,   u8"main_ps");
	auto taa_cs                    = rassets.compile_shader_in_filesystem("src/shaders/taa.hlsl",                    lgpu::shader_stage::compute_shader, u8"main_cs");
	auto lighting_blit_ps          = rassets.compile_shader_in_filesystem("src/shaders/lighting_blit.hlsl",          lgpu::shader_stage::pixel_shader,   u8"main_ps");

	auto envmap_lut = rassets.get_image2d(lren::assets::identifier(rassets.asset_library_path / "envmap_lut.dds"), runtime_tex_pool);
	auto sky_hdri = rassets.get_null_image();

	auto cam_params = lotus::camera_parameters<float>::create_look_at(zero, cvec3f(100.0f, 100.0f, 100.0f));
	camera_control<float> cam_control(cam_params);
	lotus::camera<float> prev_cam = uninitialized;

	float lighting_scale = 1.0f;
	int lighting_mode = 1;
	char sky_hdri_path[1024] = { 0 };
	float sky_scale = 1.0f;
	cvec3u32 probe_density(50, 50, 50);
	std::uint32_t direct_reservoirs_per_probe = 2;
	std::uint32_t indirect_reservoirs_per_probe = 4;
	std::uint32_t direct_sample_count_cap = 10;
	std::uint32_t indirect_sample_count_cap = 10;
	std::uint32_t indirect_spatial_reuse_passes = 3;
	auto probe_bounds = lotus::aab3f::create_from_min_max({ -10.0f, -10.0f, -10.0f }, { 10.0f, 10.0f, 10.0f });
	float visualize_probe_size = 0.1f;
	int visualize_probes_mode = 0;
	int shade_point_debug_mode = 0;
	bool trace_shadow_rays_naive = true;
	bool trace_shadow_rays_reservoir = false;
	float diffuse_mul = 1.0f;
	float specular_mul = 1.0f;
	float sh_ra_factor = 0.05f;
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
	float taa_ra_factor = 0.1f;
	int taa_sequence_x = 1;
	int taa_sequence_y = 1;
	std::uint32_t taa_sample_count = 8;
	std::uint32_t taa_sample_offset = 17;
	std::uint32_t taa_sample_param_x = 2;
	std::uint32_t taa_sample_param_y = 3;

	std::vector<cvec2f> taa_samples;
	std::uint32_t taa_phase = 0;
	auto get_taa_sample = [](int mode, int index, int param) -> float {
		switch (mode) {
		case 0:
			return 0.5f;
		case 1:
			{
				auto seq = lotus::sequences::halton<float>::create(param);
				return seq(index);
			}
		case 2:
			{
				auto seq = lotus::sequences::hammersley<float>::create();
				return seq(param, index)[0];
			}
		case 3:
			{
				auto seq = lotus::sequences::hammersley<float>::create();
				return seq(param, index)[1];
			}
		}
		return 0.0f;
	};
	auto update_taa_samples = [&]() {
		taa_samples.resize(taa_sample_count, zero);
		for (std::uint32_t i = 0; i < taa_sample_count; ++i) {
			taa_samples[i] = cvec2f(
				get_taa_sample(taa_sequence_x, i + taa_sample_offset, taa_sample_param_x),
				get_taa_sample(taa_sequence_y, i + taa_sample_offset, taa_sample_param_y)
			);
		}
	};
	update_taa_samples();

	std::uint32_t num_accumulated_frames = 0;

	shader_types::probe_constants probe_constants;

	lren::image2d_view path_tracer_accum = nullptr;
	lren::image2d_view prev_irradiance = nullptr;

	lren::structured_buffer_view direct_reservoirs = nullptr;
	lren::structured_buffer_view indirect_reservoirs = nullptr;
	lren::image3d_view probe_sh0 = nullptr;
	lren::image3d_view probe_sh1 = nullptr;
	lren::image3d_view probe_sh2 = nullptr;
	lren::image3d_view probe_sh3 = nullptr;

	auto fill_buffer = [&](lren::structured_buffer_view buf, std::uint32_t value, std::u8string_view description) {
		buf = buf.view_as<std::uint32_t>();
		shader_types::fill_buffer_constants data;
		data.size = buf.get_num_elements();
		data.value = value;
		rctx.run_compute_shader_with_thread_dimensions(
			fill_buffer_cs, cvec3u32(data.size, 1, 1),
			lren::all_resource_bindings(
				{
					{ 0, {
						{ 0, buf.bind_as_read_write() },
						{ 1, lren_bds::immediate_constant_buffer::create_for(data) },
					} },
				},
				{}
			),
			description
		);
	};
	auto fill_texture3d = [&](lren::image3d_view img, cvec4f value, std::u8string_view description) {
		shader_types::fill_texture3d_constants data;
		data.value = value;
		data.size = img.get_size();
		rctx.run_compute_shader_with_thread_dimensions(
			fill_texture3d_cs, img.get_size(),
			lren::all_resource_bindings(
				{
					{ 0, {
						{ 0, lren_bds::immediate_constant_buffer::create_for(data) },
						{ 1, img.bind_as_read_write() },
					} },
				},
				{}
			),
			description
		);
	};

	auto resize_probe_buffers = [&]() {
		uint32_t num_probes = probe_density[0] * probe_density[1] * probe_density[2];

		std::uint32_t num_direct_reservoirs = num_probes * direct_reservoirs_per_probe;
		direct_reservoirs = rctx.request_structured_buffer<shader_types::direct_lighting_reservoir>(
			u8"Direct Lighting Reservoirs", num_direct_reservoirs,
			lgpu::buffer_usage_mask::shader_read | lgpu::buffer_usage_mask::shader_write,
			runtime_buf_pool
		);
		std::uint32_t num_indirect_reservoirs = num_probes * indirect_reservoirs_per_probe;
		indirect_reservoirs = rctx.request_structured_buffer<shader_types::indirect_lighting_reservoir>(
			u8"Indirect Lighting Reservoirs", num_indirect_reservoirs,
			lgpu::buffer_usage_mask::shader_read | lgpu::buffer_usage_mask::shader_write,
			runtime_buf_pool
		);
		probe_sh0 = rctx.request_image3d(
			u8"Probe SH0", probe_density, 1, lgpu::format::r16g16b16a16_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);
		probe_sh1 = rctx.request_image3d(
			u8"Probe SH1", probe_density, 1, lgpu::format::r16g16b16a16_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);
		probe_sh2 = rctx.request_image3d(
			u8"Probe SH2", probe_density, 1, lgpu::format::r16g16b16a16_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);
		probe_sh3 = rctx.request_image3d(
			u8"Probe SH3", probe_density, 1, lgpu::format::r16g16b16a16_float,
			lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
			runtime_tex_pool
		);

		fill_buffer(direct_reservoirs, 0, u8"Clear Direct Reservoir Buffer");
		fill_buffer(indirect_reservoirs, 0, u8"Clear Indirect Reservoir Buffer");
		fill_texture3d(probe_sh0, zero, u8"Clear Probe SH0");
		fill_texture3d(probe_sh1, zero, u8"Clear Probe SH1");
		fill_texture3d(probe_sh2, zero, u8"Clear Probe SH2");
		fill_texture3d(probe_sh3, zero, u8"Clear Probe SH3");

		// compute transformation matrices
		cvec3f grid_size = probe_bounds.signed_size();
		mat33f rotscale = mat33f::diagonal(grid_size).inverse();
		mat44f world_to_grid = mat44f::identity();
		world_to_grid.set_block(0, 0, rotscale);
		world_to_grid.set_block(0, 3, rotscale * -probe_bounds.min);

		probe_constants.world_to_grid = world_to_grid;
		probe_constants.grid_to_world = world_to_grid.inverse();
		probe_constants.grid_size = probe_density;
		probe_constants.direct_reservoirs_per_probe = direct_reservoirs_per_probe;
		probe_constants.indirect_reservoirs_per_probe = indirect_reservoirs_per_probe;
	};

	resize_probe_buffers();

	auto on_resize = [&](lsys::window_events::resize &resize) {
		window_size = resize.new_size;

		swapchain.resize(window_size);
		cam_params.aspect_ratio = window_size[0] / static_cast<float>(window_size[1]);

		imgui_sctx.on_resize(resize);

		path_tracer_accum = rctx.request_image2d(u8"Path Tracer Accumulation Buffer", window_size, 1, lgpu::format::r32g32b32a32_float, lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write, runtime_tex_pool);
	};
	wnd.on_resize = [&](lsys::window_events::resize &resize) {
		on_resize(resize);
	};

	wnd.on_close_request = [&](lsys::window_events::close_request &req) {
		req.should_close = true;
		app.quit();
	};

	auto on_mouse_move = [&](lsys::window_events::mouse::move &move) {
		imgui_sctx.on_mouse_move(move);
		if (!ImGui::GetIO().WantCaptureMouse) {
			if (cam_control.on_mouse_move(move.new_position)) {
				num_accumulated_frames = 0;
			}
		}
	};
	wnd.on_mouse_move = [&](lsys::window_events::mouse::move &move) {
		on_mouse_move(move);
	};

	auto on_mouse_down = [&](lsys::window_events::mouse::button_down &down) {
		imgui_sctx.on_mouse_down(wnd, down);
		if (!ImGui::GetIO().WantCaptureMouse) {
			if (cam_control.on_mouse_down(down.button)) {
				wnd.acquire_mouse_capture();
			}
		}
	};
	wnd.on_mouse_button_down = [&](lsys::window_events::mouse::button_down &down) {
		on_mouse_down(down);
	};

	auto on_mouse_up = [&](lsys::window_events::mouse::button_up &up) {
		imgui_sctx.on_mouse_up(wnd, up);
		if (!ImGui::GetIO().WantCaptureMouse) {
			if (cam_control.on_mouse_up(up.button)) {
				wnd.release_mouse_capture();
			}
		}
	};
	wnd.on_mouse_button_up = [&](lsys::window_events::mouse::button_up &up) {
		on_mouse_up(up);
	};

	auto on_mouse_scroll = [&](lsys::window_events::mouse::scroll &sc) {
		imgui_sctx.on_mouse_scroll(sc);
	};
	wnd.on_mouse_scroll = [&](lsys::window_events::mouse::scroll &sc) {
		on_mouse_scroll(sc);
	};

	auto on_capture_broken = [&]() {
		imgui_sctx.on_capture_broken();
		cam_control.on_capture_broken();
	};
	wnd.on_capture_broken = [&]() {
		on_capture_broken();
	};

	auto on_key_down = [&](lsys::window_events::key_down &down) {
		imgui_sctx.on_key_down(down);
	};
	wnd.on_key_down = [&](lsys::window_events::key_down &down) {
		on_key_down(down);
	};

	auto on_key_up = [&](lsys::window_events::key_up &up) {
		imgui_sctx.on_key_up(up);
	};
	wnd.on_key_up = [&](lsys::window_events::key_up &up) {
		on_key_up(up);
	};

	auto on_text_input = [&](lsys::window_events::text_input &text) {
		imgui_sctx.on_text_input(text);
	};
	wnd.on_text_input = [&](lsys::window_events::text_input &text) {
		on_text_input(text);
	};


	std::default_random_engine random(std::random_device{}());
	float cpu_frame_time = 0.0f;


	wnd.show_and_activate();
	bool quit = false;
	while (!quit) {
		lsys::message_type msg_type;
		do {
			msg_type = app.process_message_nonblocking();
			quit = quit || msg_type == lsys::message_type::quit;
		} while (msg_type != lsys::message_type::none);

		if (window_size == zero) {
			continue;
		}

		auto frame_cpu_begin = std::chrono::high_resolution_clock::now();

		{
			auto frame_tmr = rctx.start_timer(u8"Frame");

			{
				auto tmr = rctx.start_timer(u8"Update Assets");
				rassets.update();
			}

			cvec2f taa_jitter = taa_samples[std::min<std::uint32_t>(taa_phase, taa_samples.size())] - cvec2f::filled(0.5f);
			auto cam = cam_params.into_camera(lotus::vec::memberwise_divide(taa_jitter, (2 * window_size).into<float>()));

			auto g_buf = lren::g_buffer::view::create(rctx, window_size, runtime_tex_pool);
			{ // g-buffer
				auto tmr = rctx.start_timer(u8"G-Buffer");

				lren::shader_types::view_data view_data;
				view_data.view                     = cam.view_matrix;
				view_data.projection               = cam.projection_matrix;
				view_data.jitter                   = cam.jitter_matrix;
				view_data.projection_view          = cam.projection_view_matrix;
				view_data.jittered_projection_view = cam.jittered_projection_view_matrix;
				view_data.prev_projection_view     = prev_cam.projection_view_matrix;
				view_data.rcp_viewport_size        = lotus::vec::memberwise_reciprocal(window_size.into<float>());

				auto buf = rctx.request_buffer(u8"View Data Constant Buffer", sizeof(view_data), lgpu::buffer_usage_mask::copy_destination | lgpu::buffer_usage_mask::shader_read, runtime_buf_pool);
				rctx.upload_buffer<lren::shader_types::view_data>(buf, { &view_data, &view_data + 1 }, 0, u8"Upload View Data");

				auto pass = g_buf.begin_pass(rctx);
				lren::g_buffer::render_instances(
					pass, scene.instances, scene.gbuffer_instance_render_details, buf.bind_as_constant_buffer()
				);
				pass.end();
			}

			auto light_diffuse = rctx.request_image2d(
				u8"Lighting Diffuse", window_size, 1, lgpu::format::r16g16b16a16_float,
				lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write | lgpu::image_usage_mask::color_render_target,
				runtime_tex_pool
			);
			auto light_specular = rctx.request_image2d(
				u8"Lighting Specular", window_size, 1, lgpu::format::r16g16b16a16_float,
				lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write,
				runtime_tex_pool
			);

			shader_types::lighting_constants lighting_constants;
			lighting_constants.jittered_projection_view         = cam.jittered_projection_view_matrix;
			lighting_constants.inverse_jittered_projection_view = cam.inverse_jittered_projection_view_matrix;
			lighting_constants.camera                           = cvec4f(cam_params.position, 1.0f);
			lighting_constants.depth_linearization_constants    = cam.depth_linearization_constants;
			lighting_constants.screen_size                      = window_size.into<std::uint32_t>();
			lighting_constants.num_lights                       = scene.lights.size();
			lighting_constants.trace_shadow_rays_for_naive      = trace_shadow_rays_naive;
			lighting_constants.trace_shadow_rays_for_reservoir  = trace_shadow_rays_reservoir;
			lighting_constants.lighting_mode                    = lighting_mode;
			lighting_constants.direct_diffuse_multiplier        = diffuse_mul;
			lighting_constants.direct_specular_multiplier       = specular_mul;
			lighting_constants.use_indirect                     = use_indirect_diffuse;
			lighting_constants.sky_scale                        = sky_scale;
			{
				cvec2f envmaplut_size = envmap_lut->image.get_size().into<float>();
				cvec2f rcp_size = lotus::vec::memberwise_reciprocal(envmaplut_size);
				lighting_constants.envmaplut_uvscale = lotus::vec::memberwise_multiply(envmaplut_size - cvec2f::filled(1.0f), rcp_size);
				lighting_constants.envmaplut_uvbias  = lotus::vec::memberwise_multiply(cvec2f::filled(0.5f), rcp_size);
			}

			uint32_t num_probes = probe_density[0] * probe_density[1] * probe_density[2];
			std::uint32_t num_indirect_reservoirs = num_probes * indirect_reservoirs_per_probe;
			auto spatial_indirect_reservoirs1 = rctx.request_structured_buffer<shader_types::indirect_lighting_reservoir>(
				u8"Indirect Lighting Reservoirs", num_indirect_reservoirs,
				lgpu::buffer_usage_mask::shader_read | lgpu::buffer_usage_mask::shader_write,
				runtime_buf_pool
			);
			auto spatial_indirect_reservoirs2 = rctx.request_structured_buffer<shader_types::indirect_lighting_reservoir>(
				u8"Indirect Lighting Reservoirs", num_indirect_reservoirs,
				lgpu::buffer_usage_mask::shader_read | lgpu::buffer_usage_mask::shader_write,
				runtime_buf_pool
			);

			if (update_probes || update_probes_this_frame) {
				update_probes_this_frame = false;

				{ // direct probes
					auto tmr = rctx.start_timer(u8"Update Direct Probes");

					shader_types::direct_reservoir_update_constants direct_update_constants;
					direct_update_constants.num_lights       = scene.lights.size();
					direct_update_constants.sample_count_cap = direct_sample_count_cap;
					direct_update_constants.frame_index      = frame_index;

					lren::all_resource_bindings resources(
						{},
						{
							{ u8"probe_consts",      lren_bds::immediate_constant_buffer::create_for(probe_constants) },
							{ u8"constants",         lren_bds::immediate_constant_buffer::create_for(direct_update_constants) },
							{ u8"direct_reservoirs", direct_reservoirs.bind_as_read_write() },
							{ u8"all_lights",        scene.lights_buffer.bind_as_read_only() },
							{ u8"rtas",              scene.tlas },
						}
					);
					rctx.run_compute_shader_with_thread_dimensions(
						direct_update_cs, probe_density, std::move(resources), u8"Update Direct Probes"
					);
				}

				{ // indirect probes
					auto tmr = rctx.start_timer(u8"Update Indirect Probes");

					shader_types::indirect_reservoir_update_constants indirect_update_constants;
					indirect_update_constants.frame_index      = frame_index;
					indirect_update_constants.sample_count_cap = indirect_sample_count_cap;
					indirect_update_constants.sky_scale        = sky_scale;
					indirect_update_constants.temporal_reuse   = indirect_temporal_reuse;

					lren::all_resource_bindings resources(
						{
							{ 8, rassets.get_samplers() },
						},
						{
							{ u8"probe_consts",    lren_bds::immediate_constant_buffer::create_for(probe_constants) },
							{ u8"constants",       lren_bds::immediate_constant_buffer::create_for(indirect_update_constants) },
							{ u8"lighting_consts", lren_bds::immediate_constant_buffer::create_for(lighting_constants) },
							{ u8"direct_probes",   direct_reservoirs.bind_as_read_only() },
							{ u8"indirect_sh0",    probe_sh0.bind_as_read_only() },
							{ u8"indirect_sh1",    probe_sh1.bind_as_read_only() },
							{ u8"indirect_sh2",    probe_sh2.bind_as_read_only() },
							{ u8"indirect_sh3",    probe_sh3.bind_as_read_only() },
							{ u8"indirect_probes", indirect_reservoirs.bind_as_read_write() },
							{ u8"rtas",            scene.tlas },
							{ u8"sky_latlong",     sky_hdri->image.bind_as_read_only() },
							{ u8"envmap_lut",      envmap_lut->image.bind_as_read_only() },
							{ u8"textures",        rassets.get_images() },
							{ u8"positions",       scene.vertex_buffers },
							{ u8"normals",         scene.normal_buffers },
							{ u8"tangents",        scene.tangent_buffers },
							{ u8"uvs",             scene.uv_buffers },
							{ u8"indices",         scene.index_buffers },
							{ u8"instances",       scene.instances_buffer.bind_as_read_only() },
							{ u8"geometries",      scene.geometries_buffer.bind_as_read_only() },
							{ u8"materials",       scene.materials_buffer.bind_as_read_only() },
							{ u8"all_lights",      scene.lights_buffer.bind_as_read_only() },
						}
					);
					rctx.run_compute_shader_with_thread_dimensions(
						indirect_update_cs, probe_density, std::move(resources), u8"Update Indirect Probes"
					);
				}

				if (indirect_spatial_reuse) { // indirect spatial reuse
					auto tmr = rctx.start_timer(u8"Indirect Spatial Reuse");

					cvec3i offsets[6] = {
						{  1,  0,  0 },
						{ -1,  0,  0 },
						{  0,  1,  0 },
						{  0, -1,  0 },
						{  0,  0,  1 },
						{  0,  0, -1 },
					};

					for (std::uint32_t i = 0; i < 3; ++i) {
						shader_types::indirect_spatial_reuse_constants reuse_constants;
						reuse_constants.offset               = offsets[i * 2 + random() % 2];
						reuse_constants.frame_index          = frame_index;
						reuse_constants.visibility_test_mode = indirect_spatial_reuse_visibility_test_mode;

						lren::all_resource_bindings resources(
							{},
							{
								{ u8"rtas",              scene.tlas },
								{ u8"input_reservoirs",  (i == 0 ? indirect_reservoirs : spatial_indirect_reservoirs1).bind_as_read_only() },
								{ u8"output_reservoirs", spatial_indirect_reservoirs2.bind_as_read_write() },
								{ u8"probe_consts",      lren_bds::immediate_constant_buffer::create_for(probe_constants) },
								{ u8"constants",         lren_bds::immediate_constant_buffer::create_for(reuse_constants) },
							}
						);
						rctx.run_compute_shader_with_thread_dimensions(indirect_spatial_reuse_cs, probe_density, std::move(resources), u8"Spatial Indirect Reuse");
						std::swap(spatial_indirect_reservoirs1, spatial_indirect_reservoirs2);
					}
				} else {
					spatial_indirect_reservoirs1 = indirect_reservoirs;
				}

				{ // summarize probes
					auto tmr = rctx.start_timer(u8"Summarize Probes");

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
							{ u8"probe_consts",        lren_bds::immediate_constant_buffer::create_for(probe_constants) },
							{ u8"constants",           lren_bds::immediate_constant_buffer::create_for(constants) },
						}
					);
					rctx.run_compute_shader_with_thread_dimensions(
						summarize_probes_cs, probe_density, std::move(resources), u8"Summarize Probes"
					);
				}
			}

			{ // lighting
				auto tmr = rctx.start_timer(u8"Direct & Indirect Diffuse");

				lren::all_resource_bindings resources(
					{
						{ 1, rassets.get_samplers() },
					},
					{
						{ u8"gbuffer_albedo_glossiness", g_buf.albedo_glossiness.bind_as_read_only() },
						{ u8"gbuffer_normal",            g_buf.normal.bind_as_read_only() },
						{ u8"gbuffer_metalness",         g_buf.metalness.bind_as_read_only() },
						{ u8"gbuffer_depth",             g_buf.depth_stencil.bind_as_read_only() },
						{ u8"out_diffuse",               light_diffuse.bind_as_read_write() },
						{ u8"out_specular",              light_specular.bind_as_read_write() },
						{ u8"all_lights",                scene.lights_buffer.bind_as_read_only() },
						{ u8"direct_reservoirs",         direct_reservoirs.bind_as_read_only() },
						{ u8"indirect_sh0",              probe_sh0.bind_as_read_only() },
						{ u8"indirect_sh1",              probe_sh1.bind_as_read_only() },
						{ u8"indirect_sh2",              probe_sh2.bind_as_read_only() },
						{ u8"indirect_sh3",              probe_sh3.bind_as_read_only() },
						{ u8"rtas",                      scene.tlas },
						{ u8"constants",                 lren_bds::immediate_constant_buffer::create_for(lighting_constants) },
						{ u8"probe_consts",              lren_bds::immediate_constant_buffer::create_for(probe_constants) },
						{ u8"sky_latlong",               sky_hdri->image.bind_as_read_only() },
					}
				);
				rctx.run_compute_shader_with_thread_dimensions(
					lighting_cs, cvec3u32(window_size.into<std::uint32_t>(), 1),
					std::move(resources), u8"Lighting"
				);
			}

			{ // sky
				auto tmr = rctx.start_timer(u8"Sky");

				shader_types::sky_constants constants;
				{
					auto rot_only = mat44f::identity();
					rot_only.set_block(0, 0, cam.view_matrix.block<3, 3>(0, 0));
					constants.inverse_projection_view_no_translation = (cam.projection_matrix * rot_only).inverse();

					auto prev_rot_only = mat44f::identity();
					prev_rot_only.set_block(0, 0, prev_cam.view_matrix.block<3, 3>(0, 0));
					constants.prev_projection_view_no_translation = prev_cam.projection_matrix * prev_rot_only;
				}
				constants.znear     = cam_params.near_plane;
				constants.sky_scale = sky_scale;

				lren::all_resource_bindings resources(
					{
						{ 1, rassets.get_samplers() },
					},
					{
						{ u8"sky_latlong", sky_hdri->image.bind_as_read_only() },
						{ u8"constants",   lren_bds::immediate_constant_buffer::create_for(constants) },
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
				
				auto pass = rctx.begin_pass(
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

			auto indirect_specular = rctx.request_image2d(u8"Indirect Specular", window_size, 1, lgpu::format::r32g32b32a32_float, lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write, runtime_tex_pool);

			{ // indirect specular
				auto tmr = rctx.start_timer(u8"Indirect Specular");

				shader_types::indirect_specular_constants constants;
				constants.enable_mis                        = enable_indirect_specular_mis;
				constants.use_screenspace_samples           = use_ss_indirect_specular;
				constants.frame_index                       = frame_index;
				constants.approx_indirect_indirect_specular = approx_indirect_indirect_specular;
				constants.use_approx_for_everything         = debug_approx_for_indirect;
				lren::all_resource_bindings resources(
					{
						{ 8, rassets.get_samplers() },
					},
					{
						{ u8"probe_consts",              lren_bds::immediate_constant_buffer::create_for(probe_constants) },
						{ u8"constants",                 lren_bds::immediate_constant_buffer::create_for(constants) },
						{ u8"lighting_consts",           lren_bds::immediate_constant_buffer::create_for(lighting_constants) },
						{ u8"direct_probes",             direct_reservoirs.bind_as_read_only() },
						{ u8"indirect_probes",           spatial_indirect_reservoirs1.bind_as_read_only() },
						{ u8"indirect_sh0",              probe_sh0.bind_as_read_only() },
						{ u8"indirect_sh1",              probe_sh1.bind_as_read_only() },
						{ u8"indirect_sh2",              probe_sh2.bind_as_read_only() },
						{ u8"indirect_sh3",              probe_sh3.bind_as_read_only() },
						{ u8"diffuse_lighting",          light_diffuse.bind_as_read_only() },
						{ u8"envmap_lut",                envmap_lut->image.bind_as_read_only() },
						{ u8"out_specular",              indirect_specular.bind_as_read_write() },
						{ u8"rtas",                      scene.tlas },
						{ u8"gbuffer_albedo_glossiness", g_buf.albedo_glossiness.bind_as_read_only() },
						{ u8"gbuffer_normal",            g_buf.normal.bind_as_read_only() },
						{ u8"gbuffer_metalness",         g_buf.metalness.bind_as_read_only() },
						{ u8"gbuffer_depth",             g_buf.depth_stencil.bind_as_read_only() },
						{ u8"sky_latlong",               sky_hdri->image.bind_as_read_only() },
						{ u8"textures",                  rassets.get_images() },
						{ u8"positions",                 scene.vertex_buffers },
						{ u8"normals",                   scene.normal_buffers },
						{ u8"tangents",                  scene.tangent_buffers },
						{ u8"uvs",                       scene.uv_buffers },
						{ u8"indices",                   scene.index_buffers },
						{ u8"instances",                 scene.instances_buffer.bind_as_read_only() },
						{ u8"geometries",                scene.geometries_buffer.bind_as_read_only() },
						{ u8"materials",                 scene.materials_buffer.bind_as_read_only() },
						{ u8"all_lights",                scene.lights_buffer.bind_as_read_only() },
					}
				);

				rctx.run_compute_shader_with_thread_dimensions(
					indirect_specular_use_visible_normals ? indirect_specular_vndf_cs : indirect_specular_cs,
					cvec3u32(window_size.into<std::uint32_t>(), 1),
					std::move(resources), u8"Indirect Specular"
				);
			}

			if (shade_point_debug_mode != 0) {
				float tan_half_fovy = std::tan(0.5f * cam_params.fov_y_radians);
				cvec3f half_right = cam.unit_right * cam_params.aspect_ratio * tan_half_fovy;
				cvec3f half_down = cam.unit_up * -tan_half_fovy;
				cvec3f pixel_x = half_right / (0.5f * window_size[0]);
				cvec3f pixel_y = half_down / (0.5f * window_size[1]);

				shader_types::shade_point_debug_constants constants;
				constants.camera      = cvec4f(cam_params.position, 1.0f);
				constants.x           = cvec4f(pixel_x, 0.0f);
				constants.y           = cvec4f(pixel_y, 0.0f);
				constants.top_left    = cvec4f(cam.unit_forward - half_right - half_down, 0.0f);
				constants.window_size = window_size.into<std::uint32_t>();
				constants.num_lights  = scene.lights.size();
				constants.mode        = shade_point_debug_mode;
				constants.num_frames  = ++num_accumulated_frames;

				lren::all_resource_bindings resources(
					{
						{ 8, rassets.get_samplers() },
					},
					{
						{ u8"probe_consts",    lren_bds::immediate_constant_buffer::create_for(probe_constants) },
						{ u8"constants",       lren_bds::immediate_constant_buffer::create_for(constants) },
						{ u8"lighting_consts", lren_bds::immediate_constant_buffer::create_for(lighting_constants) },
						{ u8"direct_probes",   direct_reservoirs.bind_as_read_only() },
						{ u8"indirect_sh0",    probe_sh0.bind_as_read_only() },
						{ u8"indirect_sh1",    probe_sh1.bind_as_read_only() },
						{ u8"indirect_sh2",    probe_sh2.bind_as_read_only() },
						{ u8"indirect_sh3",    probe_sh3.bind_as_read_only() },
						{ u8"envmap_lut",      envmap_lut->image.bind_as_read_only() },
						{ u8"out_irradiance",  light_diffuse.bind_as_read_write() },
						{ u8"out_accum",       path_tracer_accum.bind_as_read_write() },
						{ u8"rtas",            scene.tlas },
						{ u8"textures",        rassets.get_images() },
						{ u8"positions",       scene.vertex_buffers },
						{ u8"normals",         scene.normal_buffers },
						{ u8"tangents",        scene.tangent_buffers },
						{ u8"uvs",             scene.uv_buffers },
						{ u8"indices",         scene.index_buffers },
						{ u8"instances",       scene.instances_buffer.bind_as_read_only() },
						{ u8"geometries",      scene.geometries_buffer.bind_as_read_only() },
						{ u8"materials",       scene.materials_buffer.bind_as_read_only() },
						{ u8"all_lights",      scene.lights_buffer.bind_as_read_only() },
					}
				);
				rctx.run_compute_shader_with_thread_dimensions(
					shade_point_debug_cs, cvec3u32(window_size.into<std::uint32_t>(), 1),
					std::move(resources), u8"Shade Point Debug"
				);
			}

			auto irradiance = rctx.request_image2d(u8"Previous Irradiance", window_size, 1, lgpu::format::r16g16b16a16_float, lgpu::image_usage_mask::shader_read | lgpu::image_usage_mask::shader_write, runtime_tex_pool);

			{ // TAA
				auto tmr = rctx.start_timer(u8"TAA");

				lren::graphics_pipeline_state state(
					{ lgpu::render_target_blend_options::disabled() },
					nullptr,
					nullptr
				);
				shader_types::taa_constants constants;
				constants.viewport_size         = window_size.into<std::uint32_t>();
				constants.rcp_viewport_size     = lotus::vec::memberwise_reciprocal(window_size.into<float>());
				constants.use_indirect_specular = use_indirect_specular;
				constants.ra_factor             = taa_ra_factor;
				constants.enable_taa            = enable_taa && prev_irradiance.is_valid();
				lren::all_resource_bindings resources(
					{
						{ 1, rassets.get_samplers() },
					},
					{
						{ u8"diffuse_lighting",  light_diffuse.bind_as_read_only() },
						{ u8"specular_lighting", light_specular.bind_as_read_only() },
						{ u8"indirect_specular", indirect_specular.bind_as_read_only() },
						{ u8"prev_irradiance",   prev_irradiance ? prev_irradiance.bind_as_read_only() : rassets.get_invalid_image()->image.bind_as_read_only() },
						{ u8"motion_vectors",    g_buf.velocity.bind_as_read_only() },
						{ u8"out_irradiance",    irradiance.bind_as_read_write() },
						{ u8"constants",         lren_bds::immediate_constant_buffer::create_for(constants) },
					}
				);

				rctx.run_compute_shader_with_thread_dimensions(
					taa_cs, cvec3u32(window_size.into<std::uint32_t>(), 1), std::move(resources), u8"TAA"
				);

				prev_irradiance = irradiance;
			}

			{ // lighting blit
				shader_types::lighting_blit_constants constants;
				constants.lighting_scale = lighting_scale;
				lren::all_resource_bindings resources(
					{},
					{
						{ u8"constants",  lren_bds::immediate_constant_buffer::create_for(constants) },
						{ u8"irradiance", (shade_point_debug_mode != 0 ? light_diffuse : irradiance).bind_as_read_only() },
					}
				);

				lren::graphics_pipeline_state pipeline(
					{ lgpu::render_target_blend_options::disabled() },
					lgpu::rasterizer_options(lgpu::depth_bias_options::disabled(), lgpu::front_facing_mode::clockwise, lgpu::cull_mode::none, false),
					lgpu::depth_stencil_options(false, false, lgpu::comparison_function::always, false, 0, 0, lgpu::stencil_options::always_pass_no_op(), lgpu::stencil_options::always_pass_no_op())
				);

				auto pass = rctx.begin_pass(
					{ lren::image2d_color(swapchain, lgpu::color_render_target_access::create_discard_then_write()) },
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
							{ 1, rassets.get_samplers() },
						},
						{
							{ u8"gbuffer_albedo_glossiness", g_buf.albedo_glossiness.bind_as_read_only() },
							{ u8"gbuffer_normal",            g_buf.normal.bind_as_read_only() },
							{ u8"gbuffer_metalness",         g_buf.metalness.bind_as_read_only() },
							{ u8"gbuffer_depth",             g_buf.depth_stencil.bind_as_read_only() },
							{ u8"constants",                 lren_bds::immediate_constant_buffer::create_for(constants) },
						}
					);

					auto pass = rctx.begin_pass(
						{ lren::image2d_color(swapchain, lgpu::color_render_target_access::create_discard_then_write()) },
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
							{ u8"probe_consts", lren_bds::immediate_constant_buffer::create_for(probe_constants) },
							{ u8"constants",    lren_bds::immediate_constant_buffer::create_for(constants) },
							{ u8"probe_sh0",    probe_sh0.bind_as_read_only() },
							{ u8"probe_sh1",    probe_sh1.bind_as_read_only() },
							{ u8"probe_sh2",    probe_sh2.bind_as_read_only() },
							{ u8"probe_sh3",    probe_sh3.bind_as_read_only() },
						}
					);

					std::uint32_t num_probes = probe_density[0] * probe_density[1] * probe_density[2];

					auto pass = rctx.begin_pass(
						{ lren::image2d_color(swapchain, lgpu::color_render_target_access::create_preserve_and_write()) },
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

				for (const auto &l : scene.lights) {
					debug_render.add_locator(l.position, lotus::linear_rgba_f(1.0f, 0.0f, 0.0f, 1.0f));
				}

				{ // debug drawing
					debug_render.flush(
						lren::image2d_color(swapchain, lgpu::color_render_target_access::create_preserve_and_write()),
						lren::image2d_depth_stencil(g_buf.depth_stencil, lgpu::depth_render_target_access::create_preserve_and_write()),
						window_size, cam.projection_view_matrix
					);
				}

				{ // dear ImGui
					ImGui::NewFrame();

					bool needs_resizing = false;

					if (ImGui::Begin("Controls")) {
						ImGui::SliderFloat("Lighting Scale", &lighting_scale, 0.01f, 100.0f, "%.02f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
						ImGui::InputText("Sky HDRI Path", sky_hdri_path, std::size(sky_hdri_path));
						if (ImGui::Button("Load HDRI")) {
							if (std::filesystem::exists(sky_hdri_path)) {
								sky_hdri = rassets.get_image2d(lren::assets::identifier(sky_hdri_path), scene.geom_texture_pool);
							}
						}
						ImGui::SameLine();
						if (ImGui::Button("Unload HDRI")) {
							sky_hdri = rassets.get_null_image();
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
							regen_sequence = ImGui_SliderT<std::uint32_t>("TAA Sequence Offset", &taa_sample_offset, 1, 512) || regen_sequence;
							regen_sequence = ImGui_SliderT<std::uint32_t>("TAA Sequence X Param", &taa_sample_param_x, 1, 32) || regen_sequence;
							regen_sequence = ImGui_SliderT<std::uint32_t>("TAA Sequence Y Param", &taa_sample_param_y, 1, 32) || regen_sequence;
							regen_sequence = ImGui_SliderT<std::uint32_t>("TAA Sequence Length", &taa_sample_count, 1, 512, nullptr, ImGuiSliderFlags_Logarithmic) || regen_sequence;
							if (regen_sequence) {
								update_taa_samples();
							}
						}
						{ // draw samples
							constexpr cvec2f canvas_size(150.0f, 150.0f);
							constexpr float dot_radius = 2.0f;

							auto to_imvec2 = [](cvec2f p) {
								return ImVec2(p[0], p[1]);
							};
							auto to_cvec2f = [](ImVec2 p) {
								return cvec2f(p.x, p.y);
							};

							auto *list = ImGui::GetWindowDrawList();
							auto pos = to_cvec2f(ImGui::GetCursorScreenPos());
							list->AddRectFilled(to_imvec2(pos), to_imvec2(pos + canvas_size), ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));

							for (std::size_t i = 0; i < taa_samples.size(); ++i) {
								cvec2f smp = taa_samples[i];
								cvec2f dot_pos = pos + lotus::vec::memberwise_multiply(smp, canvas_size);
								float seq_pos = i / static_cast<float>(taa_samples.size() - 1);
								list->AddCircleFilled(to_imvec2(dot_pos), dot_radius, ImGui::ColorConvertFloat4ToU32(ImVec4(seq_pos, 0.0f, 1.0f - seq_pos, 1.0f)));
							}

							ImGui::SetCursorScreenPos(to_imvec2(pos + cvec2f(0.0f, canvas_size[1])));
						}
						ImGui::Separator();

						ImGui::Combo("Visualize Probes", &visualize_probes_mode, "None\0Specular\0Diffuse\0Normal\0");
						ImGui::SliderFloat("Visualize Probes Size", &visualize_probe_size, 0.0f, 1.0f);
						{
							auto probes_int = probe_density.into<int>();
							int probes[3] = { probes_int[0], probes_int[1], probes_int[2] };
							needs_resizing = ImGui::SliderInt3("Num Probes", probes, 2, 100) || needs_resizing;
							probe_density = cvec3u32(probes[0], probes[1], probes[2]);
						}
						needs_resizing = ImGui_SliderT<std::uint32_t>("Direct Reservoirs Per Probe", &direct_reservoirs_per_probe, 1, 20) || needs_resizing;
						needs_resizing = ImGui_SliderT<std::uint32_t>("Indirect Reservoirs Per Probe", &indirect_reservoirs_per_probe, 1, 20) || needs_resizing;
						{
							float rx[2] = { probe_bounds.min[0], probe_bounds.max[0] };
							float ry[2] = { probe_bounds.min[1], probe_bounds.max[1] };
							float rz[2] = { probe_bounds.min[2], probe_bounds.max[2] };
							needs_resizing = ImGui::SliderFloat2("Range X", rx, -20.0f, 20.0f) || needs_resizing;
							needs_resizing = ImGui::SliderFloat2("Range Y", ry, -20.0f, 20.0f) || needs_resizing;
							needs_resizing = ImGui::SliderFloat2("Range Z", rz, -20.0f, 20.0f) || needs_resizing;
							probe_bounds = lotus::aab3f::create_from_min_max({ rx[0], ry[0], rz[0] }, { rx[1], ry[1], rz[1] });
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
						ImGui_SliderT<std::uint32_t>("Indirect Spatial Reuse Passes", &indirect_spatial_reuse_passes, 1, 3);
						ImGui::Combo("Indirect Spatial Reuse Visibility Test Mode", &indirect_spatial_reuse_visibility_test_mode, "None\0Simple\0Full\0");
						ImGui::SliderFloat("SH RA Factor", &sh_ra_factor, 0.0f, 1.0f);
						ImGui_SliderT<std::uint32_t>("Direct Sample Count Cap", &direct_sample_count_cap, 1, 10000, "%d", ImGuiSliderFlags_Logarithmic);
						ImGui_SliderT<std::uint32_t>("Indirect Sample Count Cap", &indirect_sample_count_cap, 1, 10000, "%d", ImGuiSliderFlags_Logarithmic);
					}
					ImGui::End();

					if (ImGui::Begin("Statistics")) {
						ImGui::Text("CPU: %f ms", cpu_frame_time);

						ImGui::Separator();
						ImGui::Text("GPU Timers");
						if (ImGui::BeginTable("TimersTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableSetupColumn("Name");
							ImGui::TableSetupColumn("Duration (ms)");
							ImGui::TableHeadersRow();

							for (const auto &t : batch_stats_late.timer_results) {
								ImGui::TableNextRow();
								ImGui::TableNextColumn();
								std::string n(lstr::to_generic(t.name));
								ImGui::Text("%s", n.c_str());

								ImGui::TableNextColumn();
								ImGui::Text("%f", t.duration_ms);
							}

							ImGui::EndTable();
						}

						ImGui::Separator();
						ImGui::Text("Constant Buffer Uploads");
						if (ImGui::BeginTable("CBTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableSetupColumn("Size");
							ImGui::TableSetupColumn("Size Without Padding");
							ImGui::TableSetupColumn("Num Buffers");
							ImGui::TableHeadersRow();

							for (const auto &b : batch_stats_early.immediate_constant_buffers) {
								ImGui::TableNextRow();
								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32, b.immediate_constant_buffer_size);

								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32, b.immediate_constant_buffer_size_no_padding);

								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32, b.num_immediate_constant_buffers);
							}

							ImGui::EndTable();
						}

						ImGui::Separator();
						ImGui::Text("Constant Buffers");
						if (ImGui::BeginTable("CBSigTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableSetupColumn("Count");
							ImGui::TableSetupColumn("Hash");
							ImGui::TableSetupColumn("Size");
							ImGui::TableHeadersRow();

							std::size_t keep_count = 5;
							std::vector<sig_pair> heap;
							std::greater<sig_pair> pred;
							for (auto &&[sig, count] : batch_stats_early.constant_buffer_counts) {
								heap.emplace_back(sig, count);
								std::push_heap(heap.begin(), heap.end(), pred);
								if (heap.size() > keep_count) {
									std::pop_heap(heap.begin(), heap.end(), pred);
									heap.pop_back();
								}
							}

							std::sort(heap.begin(), heap.end(), pred);

							for (const auto &p : heap) {
								ImGui::TableNextRow();
								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32, p.count);

								ImGui::TableNextColumn();
								ImGui::Text("0x%08" PRIX32, p.signature.hash);

								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32, p.signature.size);
							}

							ImGui::EndTable();
						}

						ImGui::Separator();
						ImGui::Text("Transitions");
						if (ImGui::BeginTable("TransitionTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableSetupColumn("Image2D");
							ImGui::TableSetupColumn("Image3D");
							ImGui::TableSetupColumn("Buffer");
							ImGui::TableSetupColumn("Raw Buffer");
							ImGui::TableHeadersRow();

							for (const auto &t : batch_stats_early.transitions) {
								ImGui::TableNextRow();
								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32 " (%" PRIu32 ")", t.submitted_image2d_transitions, t.requested_image2d_transitions);

								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32 " (%" PRIu32 ")", t.submitted_image3d_transitions, t.requested_image3d_transitions);

								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32 " (%" PRIu32 ")", t.submitted_buffer_transitions, t.requested_buffer_transitions);

								ImGui::TableNextColumn();
								ImGui::Text("%" PRIu32 " (%" PRIu32 ")", t.submitted_raw_buffer_transitions, t.requested_raw_buffer_transitions);
							}

							ImGui::EndTable();
						}
					}
					ImGui::End();

					if (needs_resizing) {
						resize_probe_buffers();
					}

					ImGui::Render();
					imgui_rctx.render(
						lren::image2d_color(swapchain, lgpu::color_render_target_access::create_preserve_and_write()),
						window_size, runtime_buf_pool
					);
				}
			}

			rctx.present(swapchain, u8"Present");


			// update
			prev_cam = cam;
			++frame_index;
			taa_phase = (taa_phase + 1) % taa_samples.size();
		}

		batch_stats_early = rctx.flush();

		auto frame_cpu_end = std::chrono::high_resolution_clock::now();
		cpu_frame_time = std::chrono::duration<float, std::milli>(frame_cpu_end - frame_cpu_begin).count();
	}

	return 0;
}
