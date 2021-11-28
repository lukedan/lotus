#include <fstream>

#include <nlohmann/json.hpp>

#include <lotus/system/application.h>
#include <lotus/system/window.h>
#include <lotus/graphics/context.h>
#include <lotus/graphics/device.h>

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

	auto close_request = wnd.on_close_request.create_linked_node(
		[&app](lsys::window&, lsys::window_events::close_request &request) {
			request.should_close = true;
			app.quit();
		}
	);

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
	lgfx::command_allocator cmd_alloc = gdev.create_command_allocator();
	lgfx::descriptor_pool desc_pool = gdev.create_descriptor_pool(
		{
			lgfx::descriptor_range::create(lgfx::descriptor_type::read_only_image, 1000),
			lgfx::descriptor_range::create(lgfx::descriptor_type::sampler, 1000),
		},
		1000
	);

	// swap chain
	auto [swapchain, swapchain_format] = gctx.create_swap_chain_for_window(
		wnd, gdev, cmd_queue, 3, { lgfx::format::r8g8b8a8_srgb, lgfx::format::b8g8r8a8_srgb }
	);
	std::vector<lgfx::fence> frame_synchronization;
	bool needs_recreating_resources = true;
	bool reload = true;

	// generic vertex shader
	auto vert_shader_binary = load_binary_file("shaders/vertex.vs.o");
	assert(!vert_shader_binary.empty());
	lgfx::shader vert_shader = gdev.load_shader(vert_shader_binary);

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

	// global shader input
	lgfx::buffer globals_buf = gdev.create_committed_buffer(
		sizeof(pass::global_input), lgfx::heap_type::upload, lgfx::buffer_usage::mask::read_only_buffer
	);
	lgfx::descriptor_set_layout globals_layout = gdev.create_descriptor_set_layout(
		{
			lgfx::descriptor_range_binding::create(lgfx::descriptor_type::constant_buffer, 1, 0),
			lgfx::descriptor_range_binding::create(lgfx::descriptor_type::sampler, 2, 1),
		},
		lgfx::shader_stage::pixel_shader
	);
	lgfx::descriptor_set globals_set = gdev.create_descriptor_set(desc_pool, globals_layout);
	gdev.write_descriptor_set_constant_buffers(
		globals_set, globals_layout, 0,
		{
			lgfx::constant_buffer_view::create(globals_buf, 0, sizeof(pass::global_input)),
		}
	);
	gdev.write_descriptor_set_samplers(globals_set, globals_layout, 1, { &nearest_sampler, &linear_sampler });

	// blit pass
	auto blit_pix_shader_binary = load_binary_file("shaders/blit.ps.o");
	assert(!blit_pix_shader_binary.empty());
	lgfx::shader blit_pix_shader = gdev.load_shader(blit_pix_shader_binary);
	lgfx::descriptor_set_layout blit_descriptor_layout = gdev.create_descriptor_set_layout(
		{
			lgfx::descriptor_range_binding::create(lgfx::descriptor_type::read_only_image, 1, 0),
			lgfx::descriptor_range_binding::create(lgfx::descriptor_type::sampler, 1, 1),
		},
		lgfx::shader_stage::pixel_shader
	);
	lgfx::pipeline_resources blit_pipeline_resources = gdev.create_pipeline_resources({ &blit_descriptor_layout });
	lgfx::pass_resources blit_pass_resources = gdev.create_pass_resources(
		{
			lgfx::render_target_pass_options::create(
				swapchain_format, lgfx::pass_load_operation::clear, lgfx::pass_store_operation::preserve
			),
		},
		lgfx::depth_stencil_pass_options::create(
			lgfx::format::none,
			lgfx::pass_load_operation::discard, lgfx::pass_store_operation::discard,
			lgfx::pass_load_operation::discard, lgfx::pass_store_operation::discard
		)
	);
	std::vector<lgfx::image2d_view> blit_image_views;
	std::vector<lgfx::frame_buffer> blit_frame_buffers;
	lgfx::graphics_pipeline_state blit_pipeline_state = gdev.create_graphics_pipeline_state(
		blit_pipeline_resources,
		lgfx::shader_set::create(vert_shader, blit_pix_shader),
		{ lgfx::render_target_blend_options::disabled() },
		lgfx::rasterizer_options::create(
			lgfx::depth_bias_options::disabled(), lgfx::front_facing_mode::clockwise, lgfx::cull_mode::none
		),
		lgfx::depth_stencil_options::all_disabled(),
		{},
		lgfx::primitive_topology::triangle_strip,
		blit_pass_resources
	);
	std::array<lgfx::descriptor_set, 2> blit_descriptor_set{
		gdev.create_descriptor_set(desc_pool, blit_descriptor_layout),
		gdev.create_descriptor_set(desc_pool, blit_descriptor_layout),
	};
	for (std::size_t i = 0; i < 2; ++i) {
		gdev.write_descriptor_set_samplers(blit_descriptor_set[i], blit_descriptor_layout, 1, { &nearest_sampler });
	}

	bool is_mouse_down = false;
	lotus::cvec2s window_size = zero;
	lotus::cvec2i mouse_pos = zero;
	lotus::cvec2i mouse_down_pos = zero;
	lotus::cvec2i mouse_drag_pos = zero;

	auto recreate_resources = [&]() {
		frame_synchronization.clear();
		blit_image_views.clear();
		blit_frame_buffers.clear();

		gdev.resize_swap_chain_buffers(swapchain, window_size);

		auto cmd_list = gdev.create_and_start_command_list(cmd_alloc);
		auto fence = gdev.create_fence(lgfx::synchronization_state::unset);
		for (std::size_t i = 0; i < swapchain.get_image_count(); ++i) {
			frame_synchronization.emplace_back(gdev.create_fence(lgfx::synchronization_state::unset));
			blit_image_views.emplace_back(gdev.create_image2d_view_from(
				swapchain.get_image(i), swapchain_format, lgfx::mip_levels::only_highest()
			));
			blit_frame_buffers.emplace_back(gdev.create_frame_buffer(
				{ &blit_image_views[i] }, nullptr, window_size, blit_pass_resources
			));

			// transition all swapchain images to present state
			auto img = swapchain.get_image(i);
			gdev.set_debug_name(img, format_utf8(u8"Back buffer {}", i));
			cmd_list.resource_barrier(
				{
					lgfx::image_barrier::create(
						lgfx::subresource_index::first_color(), img,
						lgfx::image_usage::initial, lgfx::image_usage::present
					),
				},
				{}
			);
		}
		cmd_list.finish();
		cmd_queue.submit_command_lists({ &cmd_list }, &fence);
		gdev.wait_for_fence(fence);
		swapchain.update_synchronization_primitives(frame_synchronization);
	};

	auto resize = wnd.on_resize.create_linked_node(
		[&](lsys::window&, lsys::window_events::resize &info) {
			needs_recreating_resources = true;
			window_size = info.new_size;
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

	std::array<pass::output::target*, 2> main_out{ { nullptr, nullptr } };
	wnd.show_and_activate();
	while (app.process_message_nonblocking() != lsys::message_type::quit) {
		bool update_pass_output = false;
		if (reload) {
			reload = false;
			update_pass_output = true;
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
			proj.load_resources(
				gdev, shader_utils, cmd_alloc, cmd_queue,
				vert_shader, globals_layout, proj_path.parent_path(), error_callback
			);
			pass_order = proj.get_pass_order(error_callback);
		}
		if (needs_recreating_resources) {
			needs_recreating_resources = false;
			update_pass_output = true;
			recreate_resources();
		}
		if (update_pass_output) {
			for (auto &p : proj.passes) {
				p.second.create_output_images(gdev, window_size, error_callback);
			}
			proj.update_descriptor_sets(gdev, desc_pool, error_callback);
			for (std::size_t i = 0; i < 2; ++i) {
				main_out[i] = proj.find_target(proj.main_pass, i, error_callback);
				if (main_out[i]) {
					gdev.write_descriptor_set_images(
						blit_descriptor_set[i], blit_descriptor_layout,
						0, { &main_out[i]->image_view }
					);
				}
			}
		}

		auto *globals_buf_data = static_cast<pass::global_input*>(gdev.map_buffer(globals_buf, 0, 0));
		globals_buf_data->mouse = mouse_pos.into<float>();
		globals_buf_data->mouse_down = mouse_down_pos.into<float>();
		globals_buf_data->mouse_drag = mouse_drag_pos.into<float>();
		globals_buf_data->resolution = window_size.into<std::int32_t>();
		globals_buf_data->time = time;
		gdev.unmap_buffer(globals_buf, 0, sizeof(pass::global_input));

		// acquire image
		auto back_buffer = gdev.acquire_back_buffer(swapchain);
		if (back_buffer.status != lgfx::swap_chain_status::ok) {
			needs_recreating_resources = true;
			continue;
		}
		lgfx::image2d target_image = swapchain.get_image(back_buffer.index);
		if (back_buffer.on_presented) {
			gdev.wait_for_fence(*back_buffer.on_presented);
			gdev.reset_fence(*back_buffer.on_presented);
		}

		// record command list
		auto cmd_list = gdev.create_and_start_command_list(cmd_alloc);

		cmd_list.resource_barrier(
			{
				lgfx::image_barrier::create(
					lgfx::subresource_index::first_color(), target_image,
					lgfx::image_usage::present, lgfx::image_usage::color_render_target
				),
			},
			{}
		);

		// render all passes
		for (auto pit : pass_order) {
			for (auto *dep : pit->second.dependencies[frame_index]) {
				dep->transition_to(cmd_list, lgfx::image_usage::read_only_texture);
			}
			for (auto &out : pit->second.outputs[frame_index].targets) {
				out.transition_to(cmd_list, lgfx::image_usage::color_render_target);
			}
			if (pit->second.ready()) {
				std::vector<lotus::linear_rgba_f> clear_colors(
					pit->second.output_names.size(), lotus::linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f)
				);
				cmd_list.begin_pass(
					pit->second.pass_resources, pit->second.outputs[frame_index].frame_buffer, clear_colors, 0.0f, 0
				);
				cmd_list.bind_pipeline_state(pit->second.pipeline);
				cmd_list.bind_graphics_descriptor_sets(
					pit->second.pipeline_resources, 0, { &pit->second.input_descriptors[frame_index], &globals_set }
				);
				cmd_list.set_viewports({ lgfx::viewport::create(
					lotus::aab2f::create_from_min_max(zero, window_size.into<float>()), 0.0f, 1.0f
				) });
				cmd_list.set_scissor_rectangles({ lotus::aab2i::create_from_min_max(zero, window_size.into<int>()) });
				cmd_list.draw_instanced(0, 4, 0, 1);
				cmd_list.end_pass();
			}
		}

		if (main_out[frame_index]) {
			main_out[frame_index]->transition_to(cmd_list, lgfx::image_usage::read_only_texture);
			cmd_list.begin_pass(
				blit_pass_resources, blit_frame_buffers[back_buffer.index],
				{ lotus::linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f) }, 0.0f, 0
			);
			cmd_list.bind_pipeline_state(blit_pipeline_state);
			cmd_list.bind_graphics_descriptor_sets(blit_pipeline_resources, 0, { &blit_descriptor_set[frame_index] });
			cmd_list.set_viewports({ lgfx::viewport::create(
				lotus::aab2f::create_from_min_max(zero, window_size.into<float>()), 0.0f, 1.0f
			) });
			cmd_list.set_scissor_rectangles({ lotus::aab2i::create_from_min_max(zero, window_size.into<int>()) });
			cmd_list.draw_instanced(0, 4, 0, 1);
			cmd_list.end_pass();
		}

		cmd_list.resource_barrier(
			{
				lgfx::image_barrier::create(
					lgfx::subresource_index::first_color(), target_image,
					lgfx::image_usage::color_render_target, lgfx::image_usage::present
				),
			},
			{}
		);

		cmd_list.finish();

		// submit & present
		cmd_queue.submit_command_lists({ &cmd_list }, nullptr);
		if (cmd_queue.present(swapchain) != lgfx::swap_chain_status::ok) {
			needs_recreating_resources = true;
		}

		{ // wait for device to finish everything (not ideal)
			auto fence = gdev.create_fence(lgfx::synchronization_state::unset);
			cmd_queue.signal(fence);
			gdev.wait_for_fence(fence);
		}

		cmd_alloc.reset(gdev); // to avoid oom with dx12

		frame_index = (frame_index + 1) % 2;
		auto now = std::chrono::high_resolution_clock::now();
		time += std::chrono::duration<float>(now - last_frame).count();
		last_frame = now;
	}

	return 0;
}
