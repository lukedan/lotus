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
#include <lotus/renderer/context/constant_uploader.h>

#include <lotus/helpers/application.h>

#include "common.h"
#include "pass.h"
#include "project.h"

#include <imgui.cpp>
#include <imgui_draw.cpp>
#include <imgui_widgets.cpp>
#include <imgui_tables.cpp>

class shadertoy_application : public lotus::helpers::application {
public:
	shadertoy_application(int argc, char **argv) : application(argc, argv, u8"Shader Toy") {
	}
protected:
	lren::assets::handle<lren::assets::shader> _vert_shader = nullptr;
	lren::assets::handle<lren::assets::shader> _blit_pix_shader = nullptr;

	lren::context::queue _gfx_q = nullptr;
	lren::pool _resource_pool = nullptr;

	bool _mouse_down = false;
	lotus::cvec2i _mouse_pos = zero;
	lotus::cvec2i _mouse_down_pos = zero;
	lotus::cvec2i _mouse_drag_pos = zero;

	std::chrono::high_resolution_clock::time_point _start_time;
	u64 _frame_index = 0;

	char _project_path[2048] = {};
	project _project = nullptr;
	std::vector<std::map<std::u8string, pass>::iterator> _pass_order;

	constexpr static lotus::gpu::queue_family _queues[] = {
		lotus::gpu::queue_family::graphics, lotus::gpu::queue_family::copy
	};
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
		return { _assets->asset_library_path / "shaders/" };
	}

	void _on_initialized() override {
		// generic vertex shader
		_vert_shader = _assets->compile_shader_in_filesystem(
			"shaders/vertex.hlsl", lgpu::shader_stage::vertex_shader, u8"main_vs"
		);
		// blit pass
		_blit_pix_shader = _assets->compile_shader_in_filesystem(
			"shaders/blit.hlsl", lgpu::shader_stage::pixel_shader, u8"main_ps"
		);

		_gfx_q = _context->get_queue(0);
		_resource_pool = _context->request_pool(u8"Resource Pool");

		if (_argc > 1) {
			std::strcpy(_project_path, _argv[1]);
			_load_project();
		}
	}

	void _on_mouse_down(lotus::system::window_events::mouse::button_down &e) override {
		if (e.button == lsys::mouse_button::primary) {
			_window->acquire_mouse_capture();
			_mouse_down = true;
		}
	}
	void _on_mouse_up(lotus::system::window_events::mouse::button_up &e) override {
		if (e.button == lsys::mouse_button::primary) {
			if (_mouse_down) {
				_window->release_mouse_capture();
				_mouse_down = false;
			}
		}
	}
	void _on_capture_broken() override {
		_mouse_down = false;
	}
	void _on_mouse_move(lotus::system::window_events::mouse::move &e) override {
		if (_mouse_down) {
			_mouse_drag_pos += e.new_position - _mouse_pos;
			_mouse_down_pos = e.new_position;
		}
		_mouse_pos = e.new_position;
	}

	void _load_project() {
		_start_time = std::chrono::high_resolution_clock::now();
		_project = nullptr;

		lotus::log().info("Loading project");
		nlohmann::json proj_json;
		{
			std::ifstream fin(_project_path);
			try {
				fin >> proj_json;
			} catch (std::exception &ex) {
				lotus::log().error("Failed to load JSON: {}", ex.what());
			} catch (...) {
				lotus::log().error("Failed to load JSON");
			}
		}
		_project = project::load(proj_json);
		_project.load_resources(*_assets, std::filesystem::path(_project_path).parent_path(), _resource_pool);
		_pass_order = _project.get_pass_order();
	}

	void _process_frame(lotus::renderer::constant_uploader &uploader, lotus::renderer::dependency constants_dep, lotus::renderer::dependency assets_dep) override {
		// create new resources
		for (auto &p : _project.passes) {
			for (usize out_i = 0; out_i < p.second.targets.size(); ++out_i) {
				auto &out = p.second.targets[out_i];
				out.previous_frame = std::move(out.current_frame);
				auto output_name = std::format(
					"Pass \"{}\" output #{} \"{}\" frame {}",
					lstr::to_generic(p.first), out_i, lstr::to_generic(out.name), _frame_index
				);
				out.current_frame = _context->request_image2d(
					lstr::assume_utf8(output_name),
					_get_window_size(), 1, pass::output_image_format,
					lgpu::image_usage_mask::color_render_target | lgpu::image_usage_mask::shader_read,
					_resource_pool
				);
			}
		}

		pass::global_input globals_buf_data = uninitialized;
		globals_buf_data.mouse = _mouse_pos.into<float>();
		globals_buf_data.mouse_down = _mouse_down_pos.into<float>();
		globals_buf_data.mouse_drag = _mouse_drag_pos.into<float>();
		globals_buf_data.resolution = _get_window_size().into<i32>();
		globals_buf_data.time = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - _start_time).count();

		// render all passes
		for (auto pit : _pass_order) {
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
							const auto *target = _project.find_target(out.name);
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
							{ 0, uploader.upload(globals_buf_data) },
							{ 1, lren::sampler_state(
									lgpu::filtering::nearest, lgpu::filtering::nearest, lgpu::filtering::nearest
							) },
							{ 2, lren::sampler_state() },
						} },
					},
					{}
				);

				auto pass = _gfx_q.begin_pass(std::move(color_surfaces), nullptr, _get_window_size(), pit->first);
				pass.draw_instanced(
					{}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list, std::move(resource_bindings),
					_vert_shader, pit->second.shader, state, 1, pit->first
				);
				pass.end();
			}
		}

		if (auto *main_out = _project.find_target(_project.main_pass)) {
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

			auto pass = _gfx_q.begin_pass(
				{
					lren::image2d_color(
						_swap_chain, lgpu::color_render_target_access::create_clear(lotus::cvec4d(1.0, 0.0, 0.0, 0.0))
					)
				},
				nullptr, _get_window_size(), u8"Main blit pass"
			);
			pass.draw_instanced(
				{}, 3, nullptr, 0, lgpu::primitive_topology::triangle_list, std::move(resource_bindings),
				_vert_shader, _blit_pix_shader, state, 1, u8"Main blit pass"
			);
			pass.end();
		}

		++_frame_index;
	}
	void _process_imgui() override {
		if (ImGui::Begin("Shader Toy", nullptr, ImGuiWindowFlags_NoCollapse)) {
			ImGui::Text("Path");
			ImGui::PushItemWidth(-1.0f);
			ImGui::InputText("##PATH", _project_path, std::size(_project_path));
			ImGui::PopItemWidth();
			if (ImGui::Button("Reload")) {
				_load_project();
			}
		}
		ImGui::End();
	}
};

int main(int argc, char **argv) {
	shadertoy_application app(argc, argv);
	app.initialize();
	return app.run();
}
