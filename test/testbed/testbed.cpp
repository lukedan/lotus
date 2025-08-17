#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

#include <lotus/helpers/application.h>
#include <lotus/math/matrix.h>
#include <lotus/math/vector.h>
#include <lotus/math/quaternion.h>
#include <lotus/utils/camera.h>

#include "camera_control.h"
#include "utils.h"
#include "tests/box_stack_test.h"
#include "tests/cosserat_rod_test.h"
#include "tests/fem_cloth_test.h"
#include "tests/polyhedron_test.h"
#include "tests/shallow_water_test.h"
#include "tests/spring_cloth_test.h"

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
		auto depth_buf = _context->request_image2d(u8"Depth Buffer", _get_window_size(), 1, lotus::gpu::format::d32_float, lotus::gpu::image_usage_mask::depth_stencil_render_target, _pool);

		{
			auto pass = _gfx_q.begin_pass(
				{ lotus::renderer::image2d_color(_swap_chain, lotus::gpu::color_render_target_access::create_clear(lotus::cvec4d(0.5, 0.5, 1.0, 1.0))) },
				lotus::renderer::image2d_depth_stencil(depth_buf, lotus::gpu::depth_render_target_access::create_clear(0.0f)),
				_get_window_size(), u8"Clear"
			);
		}

		if (_test) {
			_test->render(
				*_context, _gfx_q, uploader,
				lotus::renderer::image2d_color(_swap_chain, lotus::gpu::color_render_target_access::create_clear(lotus::cvec4d(0.5, 0.5, 1.0, 1.0))),
				lotus::renderer::image2d_depth_stencil(depth_buf, lotus::gpu::depth_render_target_access::create_clear(0.0f)),
				_get_window_size()
			);
		}

		/*
		if (_mouse_buttons[GLFW_MOUSE_BUTTON_LEFT] || _mouse_buttons[GLFW_MOUSE_BUTTON_MIDDLE]) {
			vec3 center = _camera_params.look_at;
			glDisable(GL_LIGHTING);
			glBegin(GL_LINES);
			glColor3f(1.0f, 0.0f, 0.0f);
			glVertex3d(center[0] - 0.1, center[1], center[2]);
			glVertex3d(center[0] + 0.1, center[1], center[2]);
			glColor3f(0.0f, 1.0f, 0.0f);
			glVertex3d(center[0], center[1] - 0.1, center[2]);
			glVertex3d(center[0], center[1] + 0.1, center[2]);
			glColor3f(0.0f, 0.0f, 1.0f);
			glVertex3d(center[0], center[1], center[2] - 0.1);
			glVertex3d(center[0], center[1], center[2] + 0.1);
			glEnd();
			glEnable(GL_LIGHTING);
		}
		*/
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
		res.create = [this]() {
			return std::make_unique<Test>(_test_context);
		};
	}
protected:
	/// Used for selecting and creating tests.
	struct _test_creator {
		std::string name; ///< The name of this test.
		std::function<std::unique_ptr<test>()> create;///< Creates a test.
	};

	std::unique_ptr<test> _test; ///< The currently active test.
	usize _test_index = std::numeric_limits<usize>::max(); ///< The test that's currently selected.
	std::chrono::high_resolution_clock::time_point _last_update; ///< The time when the simulation was last updated.
	scalar _time_accum = 0.0f; ///< Accumulated time.

	bool _test_running = false; ///< Whether the test is currently running.
	float _time_scale = 100.0f; ///< Time scaling.
	float _time_step = 0.001f; ///< Time step.
	int _iters = 1; ///< Solver iterations.

	float _max_frametime = 0.1f; ///< Maximum frame time.
	scalar _simulation_speed = 0.0f; ///< Simulation speed.
	bool _update_truncated = false; ///< Whether the update was terminated early to prevent locking up.
	scalar _timestep_cost = 0.0f; ///< Running average of timestep costs.
	float _timestep_cost_factor = 0.01f; ///< Running average factor of timestep costs.

	lotus::renderer::context::queue _gfx_q = nullptr;
	lotus::renderer::pool _pool = nullptr;
	/// Sensitivity for scrolling to move the camera closer and further from the focus point.
	float _scroll_sensitivity = 0.95f;
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
				ImGui::Checkbox("Contacts", &_test_context.draw_contacts);
				ImGui::Checkbox("Particles", &_test_context.draw_particles);
				ImGui::SliderFloat("Particle Size", &_test_context.particle_size, 0.001f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
				ImGui::Checkbox("Orientations", &_test_context.draw_orientations);

				ImGui::Separator();
				ImGui::SliderFloat("Scroll Sensitivity", &_scroll_sensitivity, 0.0f, 1.0f);
				if (ImGui::Button("Reset Camera")) {
					_reset_camera();
				}
			}

			if (ImGui::CollapsingHeader("Simulation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (ImGui::BeginCombo(
					"Test", _test_index < _tests.size() ? _tests[_test_index].name.c_str() : "Select Test"
				)) {
					for (usize i = 0; i < _tests.size(); ++i) {
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
				if (ImGui::Button("Reset Test")) {
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
		_test_context.camera_params = lotus::camera_parameters<scalar>::create_look_at(lotus::zero, { 3.0f, 4.0f, 5.0f }, { 0.0f, 1.0f, 0.0f }, _get_window_size()[0] / std::max<scalar>(1.0f, static_cast<scalar>(_get_window_size()[1])));
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
		_test_context.default_shader_vs = _assets->compile_shader_in_filesystem("./shaders/default_shader.hlsl", lotus::gpu::shader_stage::vertex_shader, u8"main_vs");
		_test_context.default_shader_ps = _assets->compile_shader_in_filesystem("./shaders/default_shader.hlsl", lotus::gpu::shader_stage::pixel_shader, u8"main_ps");
		_test_context.resource_pool = _context->request_pool(u8"Test Resource Pool");
		_test_context.upload_pool = _context->request_pool(u8"Test Upload Pool", _context->get_upload_memory_type_index());
		_reset_camera();

		_gfx_q = _context->get_queue(0);
		_pool = _context->request_pool(u8"Pool");

		_camera_control = lotus::camera_control<scalar>(_test_context.camera_params);
	}

	void _on_resize(lotus::system::window_events::resize&) override {
		lotus::cvec2u32 sz = _get_window_size();
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
};

int main(int argc, char **argv) {
	testbed_app app(argc, argv);
	app.initialize();
	app.register_test<convex_hull_test>();
	app.register_test<cosserat_rod_test>();
	app.register_test<fem_cloth_test>();
	app.register_test<spring_cloth_test>();
	app.register_test<box_stack_test>();
	app.register_test<shallow_water_test>();

	return app.run();
}
