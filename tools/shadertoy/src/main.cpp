#include <fstream>

#include <nlohmann/json.hpp>

#include <lotus/logging.h>
#include <lotus/utils/misc.h>
#include <lotus/system/application.h>
#include <lotus/system/window.h>
#include <lotus/gpu/context.h>
#include <lotus/gpu/device.h>
#include <lotus/renderer/context/context.h>
#include <lotus/renderer/context/asset_manager.h>

#include "common.h"
#include "pass.h"
#include "project.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		lotus::log().info<"Usage: shadertoy [project JSON file name]">();
		return 0;
	}

	lsys::application app(u8"Shadertoy");
	lsys::window wnd = app.create_window();

	auto gctx = lgpu::context::create(lgpu::context_options::enable_validation);
	std::vector<lgpu::command_queue> gqueues;
	auto shader_utils = lgpu::shader_utility::create();
	lgpu::device gdev = nullptr;
	lgpu::adapter_properties gdev_props = uninitialized;
	gctx.enumerate_adapters([&](lgpu::adapter adap) {
		auto properties = adap.get_properties();
		if (!properties.is_discrete) {
			return true;
		}
		lotus::log().info<"Selected device: {}">(lotus::string::to_generic(properties.name));
		auto &&[dev, queues] = adap.create_device({ lgpu::queue_type::graphics });
		gdev = std::move(dev);
		gqueues = std::move(queues);
		gdev_props = properties;
		return false;
	});

	auto rctx = lren::context::create(gctx, gdev_props, gdev, gqueues);
	auto gfx_q = rctx.get_queue(0);
	auto ass_man = lren::assets::manager::create(rctx, rctx.get_queue(0), &shader_utils);
	ass_man.asset_library_path = "D:/Documents/Projects/lotus/lotus/renderer/include/lotus/renderer/assets";
	ass_man.additional_shader_include_paths = {
		ass_man.asset_library_path / "shaders/",
	};

	auto resource_pool = rctx.request_pool(u8"Resource Pool");

	// swap chain
	auto swapchain = rctx.request_swap_chain(
		u8"Swap chain", wnd, gfx_q, 3, { lgpu::format::r8g8b8a8_srgb, lgpu::format::b8g8r8a8_srgb }
	);

	bool reload = true;

	// generic vertex shader
	auto vert_shader = ass_man.compile_shader_in_filesystem(
		"shaders/vertex.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs"
	);

	// blit pass
	auto blit_pix_shader = ass_man.compile_shader_in_filesystem(
		"shaders/blit.hlsl", lgpu::shader_stage::pixel_shader, u8"main_ps"
	);


	bool is_mouse_down = false;
	lotus::cvec2u32 window_size = zero;
	lotus::cvec2i mouse_pos = zero;
	lotus::cvec2i mouse_down_pos = zero;
	lotus::cvec2i mouse_drag_pos = zero;

	auto on_close_request = [&](lsys::window_events::close_request &request) {
		window_size = zero; // prevent from resizing the frame buffers
		request.should_close = true;
		app.quit();
	};
	auto on_resize = [&](lsys::window_events::resize &info) {
		window_size = info.new_size;
		swapchain.resize(window_size);
	};
	auto on_mouse_button_down = [&](lsys::window_events::mouse::button_down &info) {
		if (info.button == lsys::mouse_button::primary) {
			wnd.acquire_mouse_capture();
			is_mouse_down = true;
		} else if (info.button == lsys::mouse_button::secondary) {
			reload = true;
		}
	};
	auto on_mouse_button_up = [&](lsys::window_events::mouse::button_up &info) {
		if (info.button == lsys::mouse_button::primary) {
			if (is_mouse_down) {
				wnd.release_mouse_capture();
				is_mouse_down = false;
			}
		}
	};
	auto on_mouse_move = [&](lsys::window_events::mouse::move &info) {
		if (is_mouse_down) {
			mouse_drag_pos += info.new_position - mouse_pos;
			mouse_down_pos = info.new_position;
		}
		mouse_pos = info.new_position;
	};

	wnd.on_close_request = [&](lsys::window_events::close_request &request) {
		on_close_request(request);
	};
	wnd.on_resize = [&](lsys::window_events::resize &info) {
		on_resize(info);
	};
	wnd.on_mouse_button_down = [&](lsys::window_events::mouse::button_down &info) {
		on_mouse_button_down(info);
	};
	wnd.on_mouse_button_up = [&](lsys::window_events::mouse::button_up &info) {
		on_mouse_button_up(info);
	};
	wnd.on_capture_broken = [&]() {
		is_mouse_down = false;
	};
	wnd.on_mouse_move = [&](lsys::window_events::mouse::move &info) {
		on_mouse_move(info);
	};

	// load project
	std::filesystem::path proj_path = argv[1];
	auto error_callback = lotus::static_function<void(std::u8string_view)>([](std::u8string_view msg) {
		lotus::log().error<u8"{}">(lotus::string::to_generic(msg));
	});
	project proj = nullptr;
	std::vector<std::map<std::u8string, pass>::iterator> pass_order;

	std::chrono::high_resolution_clock::time_point last_frame = std::chrono::high_resolution_clock::now();
	float time = 0.0f;
	std::size_t frame_index = 0;

	wnd.show_and_activate();
	while (app.process_message_nonblocking() != lsys::message_type::quit) {
		if (window_size == zero) {
			continue;
		}

		if (reload) {
			reload = false;
			time = 0.0f;
			proj = nullptr;

			lotus::log().info<u8"Loading project">();
			nlohmann::json proj_json;
			{
				std::ifstream fin(proj_path);
				try {
					fin >> proj_json;
				} catch (std::exception &ex) {
					lotus::log().error<u8"Failed to load JSON: {}">(ex.what());
				} catch (...) {
					lotus::log().error<u8"Failed to load JSON">();
				}
			}
			proj = project::load(proj_json, error_callback);
			proj.load_resources(ass_man, vert_shader, proj_path.parent_path(), resource_pool, error_callback);
			pass_order = proj.get_pass_order(error_callback);
		}

		for (auto &p : proj.passes) {
			for (std::size_t out_i = 0; out_i < p.second.targets.size(); ++out_i) {
				auto &out = p.second.targets[out_i];
				out.previous_frame = std::move(out.current_frame);
				out.current_frame = rctx.request_image2d(
					format_utf8<u8"Pass \"{}\" output #{} \"{}\" frame {}">(
						lstr::to_generic(p.first), out_i, lstr::to_generic(out.name), frame_index
					), window_size, 1, pass::output_image_format,
					lgpu::image_usage_mask::color_render_target | lgpu::image_usage_mask::shader_read,
					resource_pool
				);
			}
		}

		pass::global_input globals_buf_data = uninitialized;
		globals_buf_data.mouse = mouse_pos.into<float>();
		globals_buf_data.mouse_down = mouse_down_pos.into<float>();
		globals_buf_data.mouse_drag = mouse_drag_pos.into<float>();
		globals_buf_data.resolution = window_size.into<std::int32_t>();
		globals_buf_data.time = time;

		// render all passes
		for (auto pit : pass_order) {
			if (pit->second.ready()) {
				std::vector<lren::image2d_color> color_surfaces;
				lren::graphics_pipeline_state state(
					{},
					lgpu::rasterizer_options(
						lgpu::depth_bias_options::disabled(),
						lgpu::front_facing_mode::clockwise,
						lgpu::cull_mode::none,
						false
					),
					lgpu::depth_stencil_options::all_disabled()
				);
				for (auto &out : pit->second.targets) {
					color_surfaces.emplace_back(
						out.current_frame,
						lgpu::color_render_target_access::create_clear(lotus::cvec4d(1.0, 0.0, 0.0, 0.0))
					);
					state.blend_options.emplace_back(lgpu::render_target_blend_options::disabled());
				}
				lren::all_resource_bindings::numbered_descriptor_bindings custom_bindings;
				for (const auto &in : pit->second.inputs) {
					if (in.register_index) {
						if (std::holds_alternative<pass::input::pass_output>(in.value)) {
							const auto &out = std::get<pass::input::pass_output>(in.value);
							const auto *target = proj.find_target(out.name, error_callback);
							if (target) {
								custom_bindings.emplace_back(
									in.register_index.value(),
									(out.previous_frame ? target->previous_frame : target->current_frame).bind_as_read_only()
								);
							}
						}
					}
				}
				lren::all_resource_bindings resource_bindings(
					{
						{ 0, std::move(custom_bindings) },
						{ 1, {
							{ 0, lren::descriptor_resource::immediate_constant_buffer::create_for(globals_buf_data) },
							{ 1, lren::sampler_state(
									lgpu::filtering::nearest, lgpu::filtering::nearest, lgpu::filtering::nearest
							) },
							{ 2, lren::sampler_state() },
						} },
					},
					{}
				);

				auto pass = gfx_q.begin_pass(std::move(color_surfaces), nullptr, window_size, pit->first);
				pass.draw_instanced(
					{}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list, std::move(resource_bindings),
					vert_shader, pit->second.shader, state, 1, pit->first
				);
				pass.end();
			}
		}

		if (auto *main_out = proj.find_target(proj.main_pass, error_callback)) {
			lren::graphics_pipeline_state state(
				{ lgpu::render_target_blend_options::disabled() },
				lgpu::rasterizer_options(
					lgpu::depth_bias_options::disabled(),
					lgpu::front_facing_mode::clockwise,
					lgpu::cull_mode::none,
					false
				),
				lgpu::depth_stencil_options::all_disabled()
			);
			lren::all_resource_bindings resource_bindings(
				{
					{ 0, {
						{ 0, main_out->current_frame.bind_as_read_only() },
						{ 1, lren::sampler_state(
							lgpu::filtering::nearest, lgpu::filtering::nearest, lgpu::filtering::nearest
						) },
					} },
				},
				{}
			);

			auto pass = gfx_q.begin_pass(
				{
					lren::image2d_color(
						swapchain, lgpu::color_render_target_access::create_clear(lotus::cvec4d(1.0, 0.0, 0.0, 0.0))
					)
				},
				nullptr, window_size, u8"Main blit pass"
			);
			pass.draw_instanced(
				{}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list, std::move(resource_bindings),
				vert_shader, blit_pix_shader, state, 1, u8"Main blit pass"
			);
			pass.end();
		}

		gfx_q.present(swapchain, u8"Present");

		rctx.execute_all();

		++frame_index;
		auto now = std::chrono::high_resolution_clock::now();
		time += std::chrono::duration<float>(now - last_frame).count();
		last_frame = now;
	}

	return 0;
}
