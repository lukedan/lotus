#include <fstream>

#include <nlohmann/json.hpp>

#include <lotus/logging.h>
#include <lotus/utils/misc.h>
#include <lotus/system/application.h>
#include <lotus/system/window.h>
#include <lotus/graphics/context.h>
#include <lotus/graphics/device.h>
#include <lotus/renderer/context.h>

#include "common.h"
#include "pass.h"
#include "project.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cerr << "Usage: shadertoy [project JSON file name]\n";
		return 0;
	}

	lsys::application app(u8"Shadertoy");
	lsys::window wnd = app.create_window();

	auto gctx = lgfx::context::create();
	auto shader_utils = lgfx::shader_utility::create();
	lgfx::device gdev = nullptr;
	gctx.enumerate_adapters([&](lgfx::adapter adap) {
		auto properties = adap.get_properties();
		if (!properties.is_discrete) {
			return true;
		}
		std::cerr << "Selected device: " << reinterpret_cast<const char*>(properties.name.c_str()) << "\n";
		gdev = adap.create_device();
		return false;
	});
	lgfx::command_queue cmd_queue = gdev.create_command_queue();

	auto ass_man = lren::assets::manager::create(gdev, cmd_queue, &shader_utils);
	auto rctx = lren::context::create(ass_man, gctx, cmd_queue);

	// swap chain
	auto swapchain = rctx.request_swap_chain(
		u8"Swap chain", wnd, 3, { lgfx::format::r8g8b8a8_srgb, lgfx::format::b8g8r8a8_srgb }
	);

	bool reload = true;

	// generic vertex shader
	auto vert_shader = ass_man.compile_shader_in_filesystem(lren::assets::identifier::create("shaders/vertex.hlsl", u8"vs|main_vs")).take_ownership();

	auto nearest_sampler = gdev.create_sampler(
		lgfx::filtering::nearest, lgfx::filtering::nearest, lgfx::filtering::nearest,
		0.0f, 0.0f, 0.0f, 1.0f,
		lgfx::sampler_address_mode::repeat, lgfx::sampler_address_mode::repeat, lgfx::sampler_address_mode::repeat,
		lotus::linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f), std::nullopt
	);
	auto linear_sampler = gdev.create_sampler(
		lgfx::filtering::linear, lgfx::filtering::linear, lgfx::filtering::nearest,
		0.0f, 0.0f, 0.0f, 1.0f,
		lgfx::sampler_address_mode::repeat, lgfx::sampler_address_mode::repeat, lgfx::sampler_address_mode::repeat,
		lotus::linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f), std::nullopt
	);

	// blit pass
	auto blit_pix_shader = ass_man.compile_shader_in_filesystem(lren::assets::identifier::create("shaders/blit.hlsl", u8"ps|main_ps")).take_ownership();

	lotus::cvec2s window_size = zero;

	bool is_mouse_down = false;
	lotus::cvec2i mouse_pos = zero;
	lotus::cvec2i mouse_down_pos = zero;
	lotus::cvec2i mouse_drag_pos = zero;

	auto close_request = wnd.on_close_request.create_linked_node(
		[&](lsys::window&, lsys::window_events::close_request &request) {
			window_size = zero; // prevent from resizing the frame buffers
			request.should_close = true;
			app.quit();
		}
	);

	auto resize = wnd.on_resize.create_linked_node(
		[&](lsys::window&, lsys::window_events::resize &info) {
			window_size = info.new_size;
			swapchain.resize(window_size);
		}
	);
	auto mouse_down = wnd.on_mouse_button_down.create_linked_node(
		[&](lsys::window &w, lsys::window_events::mouse::button_down &info) {
			if (info.button == lsys::mouse_button::primary) {
				w.acquire_mouse_capture();
				is_mouse_down = true;
			} else if (info.button == lsys::mouse_button::secondary) {
				reload = true;
			}
		}
	);
	auto mouse_up = wnd.on_mouse_button_up.create_linked_node(
		[&](lsys::window &w, lsys::window_events::mouse::button_up &info) {
			if (info.button == lsys::mouse_button::primary) {
				if (is_mouse_down) {
					w.release_mouse_capture();
					is_mouse_down = false;
				}
			}
		}
	);
	auto capture_broken = wnd.on_capture_broken.create_linked_node(
		[&](lsys::window&) {
			is_mouse_down = false;
		}
	);
	struct {
		bool &is_mouse_down;
		lotus::cvec2i &mouse_drag_pos;
		lotus::cvec2i &mouse_down_pos;
		lotus::cvec2i &mouse_pos;
	} closure = { is_mouse_down, mouse_drag_pos, mouse_down_pos, mouse_pos };
	auto mouse_move = wnd.on_mouse_move.create_linked_node(
		[&](lsys::window&, lsys::window_events::mouse::move &info) {
			if (closure.is_mouse_down) {
				closure.mouse_drag_pos += info.new_position - closure.mouse_pos;
				closure.mouse_down_pos = info.new_position;
			}
			closure.mouse_pos = info.new_position;
		}
	);

	// load project
	std::filesystem::path proj_path = argv[1];
	auto error_callback = [](std::u8string_view msg) {
		std::cerr << std::string_view(reinterpret_cast<const char*>(msg.data()), msg.size()) << "\n";
	};
	project proj = nullptr;
	std::vector<std::map<std::u8string, pass>::iterator> pass_order;

	std::chrono::high_resolution_clock::time_point last_frame = std::chrono::high_resolution_clock::now();
	float time = 0.0f;
	std::size_t frame_index = 0;

	wnd.show_and_activate();
	while (app.process_message_nonblocking() != lsys::message_type::quit) {
		if (reload) {
			reload = false;
			time = 0.0f;
			proj = nullptr;

			std::cerr << "\n==============================================\nLoading project\n";
			nlohmann::json proj_json;
			{
				std::ifstream fin(proj_path);
				try {
					fin >> proj_json;
				} catch (std::exception &ex) {
					std::cerr << "Failed to load JSON: " << ex.what() << "\n";
				} catch (...) {
					std::cerr << "Failed to load JSON\n";
				}
			}
			proj = project::load(proj_json, error_callback);
			proj.load_resources(ass_man, rctx, vert_shader.duplicate(), proj_path.parent_path(), error_callback);
			pass_order = proj.get_pass_order(error_callback);
		}

		for (auto &p : proj.passes) {
			for (std::size_t out_i = 0; out_i < p.second.targets.size(); ++out_i) {
				auto &out = p.second.targets[out_i];
				out.previous_frame = std::move(out.current_frame);
				out.current_frame = rctx.request_surface2d(
					format_utf8<u8"Pass \"{}\" output #{} \"{}\" frame {}">(
						lstr::to_generic(p.first), out_i, lstr::to_generic(out.name), frame_index
					), window_size, pass::output_image_format
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
				std::vector<lren::surface2d_color> color_surfaces;
				lren::graphics_pipeline_state state(
					{},
					lgfx::rasterizer_options(
						lgfx::depth_bias_options::disabled(),
						lgfx::front_facing_mode::clockwise,
						lgfx::cull_mode::none,
						false
					),
					lgfx::depth_stencil_options::all_disabled()
				);
				for (auto &out : pit->second.targets) {
					color_surfaces.emplace_back(
						out.current_frame,
						lgfx::color_render_target_access::create_clear(lotus::cvec4d(0.0, 0.0, 0.0, 0.0))
					);
					state.blend_options.emplace_back(lgfx::render_target_blend_options::disabled());
				}
				lren::all_resource_bindings resource_bindings = nullptr;
				resource_bindings.sets = {
					lren::resource_set_binding({
						lren::resource_binding(
							lren::descriptor_resource::immediate_constant_buffer::create_for(globals_buf_data), 0
						),
						lren::resource_binding(
							lren::descriptor_resource::sampler(
								lgfx::filtering::nearest, lgfx::filtering::nearest, lgfx::filtering::nearest
							), 1
						),
						lren::resource_binding(
							lren::descriptor_resource::sampler(), 2
						),
					}, 1)
				};
				for (const auto &in : pit->second.inputs) {
					if (in.register_index) {
						if (std::holds_alternative<pass::input::pass_output>(in.value)) {
							const auto &out = std::get<pass::input::pass_output>(in.value);
							const auto *target = proj.find_target(out.name, error_callback);
							if (target) {
								resource_bindings.sets.emplace_back(std::vector({ lren::resource_binding(
									lren::descriptor_resource::image2d::create_read_only(
										out.previous_frame ? target->previous_frame : target->current_frame
									),
									in.register_index.value()
								) }), 0);
							}
						}
					}
				}
				resource_bindings.consolidate();

				auto pass = rctx.begin_pass(std::move(color_surfaces), nullptr, window_size, pit->first);
				pass.draw_instanced(
					{}, 3, nullptr, 0, lgfx::primitive_topology::triangle_list, std::move(resource_bindings),
					vert_shader.duplicate(), pit->second.shader.duplicate(), state, 1, pit->first
				);
				pass.end();
			}
		}

		if (auto *main_out = proj.find_target(proj.main_pass, error_callback)) {
			lren::graphics_pipeline_state state(
				{ lgfx::render_target_blend_options::disabled() },
				lgfx::rasterizer_options(
					lgfx::depth_bias_options::disabled(),
					lgfx::front_facing_mode::clockwise,
					lgfx::cull_mode::none,
					false
				),
				lgfx::depth_stencil_options::all_disabled()
			);
			lren::all_resource_bindings resource_bindings = nullptr;
			resource_bindings.sets = {
				lren::resource_set_binding({
					lren::resource_binding(
						lren::descriptor_resource::image2d::create_read_only(main_out->current_frame), 0
					),
				}, 0)
			};

			auto pass = rctx.begin_pass(
				{
					lren::surface2d_color(
						swapchain, lgfx::color_render_target_access::create_clear(lotus::cvec4d(0.0, 0.0, 0.0, 0.0))
					)
				},
				nullptr, window_size, u8"Main blit pass"
			);
			pass.draw_instanced(
				{}, 3, nullptr, 0, lgfx::primitive_topology::triangle_list, resource_bindings,
				vert_shader.duplicate(), blit_pix_shader.duplicate(), state, 1, u8"Main blit pass"
			);
			pass.end();
		}

		rctx.present(swapchain, u8"Present");
		rctx.flush();

		++frame_index;
		auto now = std::chrono::high_resolution_clock::now();
		time += std::chrono::duration<float>(now - last_frame).count();
		last_frame = now;
	}

	return 0;
}