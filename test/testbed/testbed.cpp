#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

#include <lotus/helpers/application.h>
#include <lotus/math/matrix.h>
#include <lotus/math/vector.h>
#include <lotus/utils/camera.h>

#include "camera_control.h"
#include "utils.h"
#include "tests/angular_momentum_test.h"
#include "tests/box_stack_test.h"
#include "tests/cosserat_rod_test.h"
#include "tests/fem_cloth_test.h"
#include "tests/hinge_test.h"
#include "tests/pin_test.h"
#include "tests/polyhedron_test.h"
#include "tests/shallow_water_test.h"
#include "tests/spring_cloth_test.h"
#include "tests/spring_test.h"

#include <imgui.cpp>
#include <imgui_draw.cpp>
#include <imgui_widgets.cpp>
#include <imgui_tables.cpp>

/// The testbed applicaiton.
class testbed_app : public lotus::helpers::application {
public:
	/// Initializes the application.
	testbed_app(int argc, char **argv) : application(argc, argv, u8"Physics Testbed") {
	}

	/// Renders all objects.
	void render(lotus::renderer::constant_uploader &uploader) {
		if (_test) {
			_test->render(*_context, _gfx_q, uploader, _swap_chain, _get_back_buffer_size());
		} else {
			auto pass = _gfx_q.begin_pass(
				{ lotus::renderer::image2d_color(_swap_chain, lotus::gpu::color_render_target_access::create_clear(lotus::zero)) },
				nullptr,
				_get_back_buffer_size(), u8"Clear"
			);
		}
	}

	/// Updates the simulation.
	void update() {
		if (_test_running) {
			auto now = std::chrono::high_resolution_clock::now();
			scalar dt = std::chrono::duration<scalar>(now - _last_update).count();
			if (_test) {
				_update_truncated = false;
				scalar target = dt * (_time_scale / 100.0f);
				scalar consumed = 0.0f;
				_time_accum += target;
				auto timestep_beg = std::chrono::high_resolution_clock::now();
				for (; _time_accum >= _time_step; _time_accum -= _time_step) {
					_test->timestep(_time_step, _iters);
					consumed += _time_step;

					auto timestep_end = std::chrono::high_resolution_clock::now();

					scalar this_cost = std::chrono::duration<scalar, std::milli>(timestep_end - timestep_beg).count();
					_timestep_cost = (1.0f - _timestep_cost_factor) * _timestep_cost + _timestep_cost_factor * this_cost;
					timestep_beg = timestep_end;

					auto frame = std::chrono::duration<scalar>(timestep_end - now).count();
					if (frame > _max_frametime) {
						_update_truncated = true;
						_time_accum = 0.0;
						break;
					}
				}
				_simulation_speed = consumed / target;
			}
			_last_update = now;
		}
	}

	template <typename Test> void register_test() {
		auto &res = _tests.emplace_back();
		res.name = std::string(Test::get_name());
		res.category = Test::get_category();
		res.create = [this]() {
			auto new_test = std::make_unique<Test>(_test_context);
			new_test->soft_reset();
			return new_test;
		};
	}
protected:
	/// Used for selecting and creating tests.
	struct _test_creator {
		std::string name; ///< The name of this test.
		test_category category;
		std::function<std::unique_ptr<test>()> create;///< Creates a test.
	};

	std::unique_ptr<test> _test; ///< The currently active test.
	usize _test_index = std::numeric_limits<usize>::max(); ///< The test that's currently selected.
	std::chrono::high_resolution_clock::time_point _last_update; ///< The time when the simulation was last updated.
	scalar _time_accum = 0.0f; ///< Accumulated time.

	bool _test_running = false; ///< Whether the test is currently running.
	f32 _time_scale = 100.0f; ///< Time scaling.
	f32 _time_step = 1.0f / 60.0f; ///< Time step.
	int _iters = 8; ///< Solver iterations.

	f32 _max_frametime = 0.1f; ///< Maximum frame time.
	scalar _simulation_speed = 0.0f; ///< Simulation speed.
	bool _update_truncated = false; ///< Whether the update was terminated early to prevent locking up.
	scalar _timestep_cost = 0.0f; ///< Running average of timestep costs.
	f32 _timestep_cost_factor = 0.01f; ///< Running average factor of timestep costs.

	lotus::renderer::context::queue _gfx_q = nullptr;
	/// Sensitivity for scrolling to move the camera closer and further from the focus point.
	f32 _scroll_sensitivity = 0.95f;
	lotus::camera_control<scalar> _camera_control = nullptr;

	test_context _test_context; ///< Test context.
	std::vector<_test_creator> _tests; ///< The list of tests.


	/// Renders and handles the GUI.
	void _process_imgui() override {
		if (ImGui::Begin("Testbed", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
			if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Checkbox("Wireframe Surfaces", &_test_context.wireframe_surfaces);
				ImGui::Checkbox("Wireframe Bodies", &_test_context.wireframe_bodies);
				ImGui::Checkbox("Body Velocity", &_test_context.draw_body_velocities);
				ImGui::Checkbox("Contact Points", &_test_context.draw_contact_points);
				ImGui::Checkbox("Contact Normals", &_test_context.draw_contact_normals);
				ImGui::Checkbox("Contact Relationships", &_test_context.draw_contact_relationships);
				ImGui::Checkbox("Particles", &_test_context.draw_particles);
				ImGui::Checkbox("Shadows", &_test_context.draw_shadows);
				ImGui::Checkbox("Faces", &_test_context.draw_faces);
				ImGui::SliderFloat("Point Size", &_test_context.point_size, 0.0f, 50.0f);
				ImGui::SliderFloat("Point Opacity", &_test_context.point_opacity, 0.0f, 1.0f);
				ImGui::Checkbox("Point Depth Testing", &_test_context.point_depth_test);
				ImGui::SliderFloat("Line Opacity", &_test_context.line_opacity, 0.0f, 1.0f);
				ImGui::Checkbox("Line Depth Test", &_test_context.line_depth_test);
				ImGui::Checkbox("Orientations", &_test_context.draw_orientations);
				ImGui::SliderFloat("SSAO Smoothing", &_test_context.ssao_smoothing, 0.0f, 1.0f);

				ImGui::Separator();
				ImGui::SliderFloat("Scroll Sensitivity", &_scroll_sensitivity, 0.0f, 1.0f);
				if (ImGui::Button("Reset Camera")) {
					_reset_camera();
				}
			}

			if (ImGui::CollapsingHeader("Simulation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (ImGui::BeginCombo(
					"Test",
					_test_index < _tests.size() ? _tests[_test_index].name.c_str() : "Select Test",
					ImGuiComboFlags_HeightLargest
				)) {
					test_category last_category = test_category::invalid;
					for (usize i = 0; i < _tests.size(); ++i) {
						if (_tests[i].category != last_category) {
							if (last_category != test_category::invalid) {
								ImGui::Separator();
							}
							last_category = _tests[i].category;
							ImGui::TextDisabled(test_category_names[std::to_underlying(last_category)]);
						}
						bool selected = _test_index == i;
						if (ImGui::Selectable(_tests[i].name.c_str(), &selected)) {
							_test_index = i;
							_test_running = false;
							_test.reset();
							_test = _tests[i].create();
						}
					}
					ImGui::EndCombo();
				}
				if (ImGui::Checkbox("Test Running", &_test_running)) {
					if (_test_running) {
						_last_update = std::chrono::high_resolution_clock::now();
					}
				}
				ImGui::SliderFloat("Time Scaling", &_time_scale, 0.0f, 100.0f, "%.1f%%");
				ImGui::SliderFloat("Time Step", &_time_step, 0.001f, 0.1f, "%.3fs", ImGuiSliderFlags_Logarithmic);
				ImGui::SliderInt("Iterations", &_iters, 1, 100);
				if (ImGui::Button("Execute Single Time Step")) {
					if (_test) {
						_test->timestep(_time_step, _iters);
					}
				}
				if (ImGui::Button("Soft Reset")) {
					if (_test) {
						_test->soft_reset();
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Hard Reset")) {
					_test_running = false;
					_test.reset();
					if (_test_index < _tests.size()) {
						_test = _tests[_test_index].create();
					}
				}
			}

			if (ImGui::CollapsingHeader("Test Specific", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (_test) {
					_test->gui();
				} else {
					ImGui::Text("[No test selected]");
				}
			}

			if (ImGui::CollapsingHeader("Simulation Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::SliderFloat("Maximum Frame Time", &_max_frametime, 0.0f, 1.0f);
				if (_update_truncated) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				}
				if (_time_scale < 100.0f) {
					ImGui::LabelText(
						"Simulation Speed", "%5.1f%% x %.1f%% = %5.1f%%",
						_simulation_speed * 100.0f, _time_scale, _simulation_speed * _time_scale
					);
				} else {
					ImGui::LabelText("Simulation Speed", "%5.1f%%", _simulation_speed * 100.0f);
				}
				if (_update_truncated) {
					ImGui::PopStyleColor();
				}

				ImGui::SliderFloat(
					"RA Timestep Factor", &_timestep_cost_factor, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic
				);
				ImGui::LabelText("RA Timestep Cost", "%.3fms", _timestep_cost);
			}
		}
		ImGui::End();
	}

	void _process_frame(lotus::renderer::constant_uploader &uploader, lotus::renderer::dependency constants_dep, lotus::renderer::dependency asset_dep) override {
		_gfx_q.acquire_dependency(constants_dep, u8"Wait for constants");
		if (asset_dep) {
			_gfx_q.acquire_dependency(asset_dep, u8"Wait for assets");
		}

		update();
		render(uploader);
	}


	void _reset_camera() {
		const vec2 window_size = _get_back_buffer_size().into<scalar>();
		_test_context.camera_params = lotus::camera_parameters<scalar>::create_look_at(
			lotus::zero,
			{ 3.0f, 4.0f, 5.0f },
			{ 0.0f, 1.0f, 0.0f },
			window_size[0] / std::max<scalar>(1.0f, window_size[1])
		);
		_test_context.update_camera();
	}

	constexpr static lotus::gpu::queue_family _queues[] = { lotus::gpu::queue_family::graphics, lotus::gpu::queue_family::copy };

	std::span<const lotus::gpu::queue_family> _get_desired_queues() const override {
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
		std::sort(_tests.begin(), _tests.end(), [](const _test_creator &lhs, const _test_creator &rhs) {
			if (lhs.category == rhs.category) {
				return lhs.name < rhs.name;
			}
			return lhs.category < rhs.category;
		});

		_test_context.gbuffer_shader_vs  = _assets->compile_shader_in_filesystem("./shaders/gbuffer_shader.hlsl", lotus::gpu::shader_stage::vertex_shader,  u8"main_vs");
		_test_context.gbuffer_shader_ps  = _assets->compile_shader_in_filesystem("./shaders/gbuffer_shader.hlsl", lotus::gpu::shader_stage::pixel_shader,   u8"main_ps");
		_test_context.point_shader_vs    = _assets->compile_shader_in_filesystem("./shaders/point_shader.hlsl",   lotus::gpu::shader_stage::vertex_shader,  u8"main_vs");
		_test_context.point_shader_ps    = _assets->compile_shader_in_filesystem("./shaders/point_shader.hlsl",   lotus::gpu::shader_stage::pixel_shader,   u8"main_ps");
		_test_context.line_shader_vs     = _assets->compile_shader_in_filesystem("./shaders/line_shader.hlsl",    lotus::gpu::shader_stage::vertex_shader,  u8"main_vs");
		_test_context.line_shader_ps     = _assets->compile_shader_in_filesystem("./shaders/line_shader.hlsl",    lotus::gpu::shader_stage::pixel_shader,   u8"main_ps");
		_test_context.shadow_vs          = _assets->compile_shader_in_filesystem("./shaders/shadow.hlsl",         lotus::gpu::shader_stage::vertex_shader,  u8"main_vs");
		_test_context.fullscreen_quad_vs = _assets->compile_shader_in_filesystem(_assets->asset_library_path / "shaders/misc/fullscreen_quad_vs.hlsl", lotus::gpu::shader_stage::vertex_shader, u8"main_vs", { { u8"FULLSCREEN_QUAD_DEPTH", u8"0" } });
		_test_context.light_quad_ps      = _assets->compile_shader_in_filesystem("./shaders/light_quad.hlsl",     lotus::gpu::shader_stage::pixel_shader,   u8"main_ps");
		_test_context.ssao_cs            = _assets->compile_shader_in_filesystem("./shaders/ssao.hlsl",           lotus::gpu::shader_stage::compute_shader, u8"main_cs");
		_test_context.sky_ps             = _assets->compile_shader_in_filesystem("./shaders/sky.hlsl",            lotus::gpu::shader_stage::pixel_shader,   u8"main_ps");

		_test_context.asset_manager = _assets.get();
		_test_context.resource_pool = _context->request_pool(u8"Test Resource Pool");
		_test_context.upload_pool = _context->request_pool(u8"Test Upload Pool", _context->get_upload_memory_type_index());

		_reset_camera();

		_gfx_q = _context->get_queue(0);

		_camera_control = lotus::camera_control<scalar>(_test_context.camera_params);
	}

	void _on_resize(lotus::system::window_events::resize&) override {
		cvec2u32 sz = _get_back_buffer_size();
		_test_context.camera_params.aspect_ratio = sz[0] / static_cast<scalar>(sz[1]);
		_test_context.update_camera();
	}
	void _on_mouse_move(lotus::system::window_events::mouse::move &move) override {
		if (_camera_control.on_mouse_move(move.new_position)) {
			_test_context.update_camera();
		}
	}
	void _on_mouse_down(lotus::system::window_events::mouse::button_down &down) override {
		_camera_control.on_mouse_down(down.button, down.modifiers);
	}
	void _on_mouse_up(lotus::system::window_events::mouse::button_up &up) override {
		_camera_control.on_mouse_up(up.button);
	}
	void _on_mouse_scroll(lotus::system::window_events::mouse::scroll &scroll) override {
		vec3 diff = _test_context.camera_params.position - _test_context.camera_params.look_at;
		diff *= std::pow(_scroll_sensitivity, scroll.offset[1]);
		_test_context.camera_params.position = _test_context.camera_params.look_at + diff;
		_test_context.update_camera();
	}
	void _on_key_down(lotus::system::window_events::key_down &kd) override {
		if (_test) {
			_test->on_key_down(kd);
		}
	}
	void _on_key_up(lotus::system::window_events::key_up &ku) override {
		if (_test) {
			_test->on_key_up(ku);
		}
	}
};

int main(int argc, char **argv) {
	testbed_app app(argc, argv);
	app.register_test<angular_momentum_test>();
	app.register_test<box_stack_test>();
	app.register_test<cosserat_rod_test>();
	app.register_test<fem_cloth_test>();
	app.register_test<hinge_test>();
	app.register_test<pin_test>();
	app.register_test<convex_hull_test>();
	app.register_test<shallow_water_test>();
	app.register_test<spring_cloth_test>();
	app.register_test<spring_test>();
	app.initialize();

	return app.run();
}
