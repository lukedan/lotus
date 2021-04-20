#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#include <pbd/math/matrix.h>
#include <pbd/math/vector.h>
#include <pbd/math/quaternion.h>
#include <pbd/camera.h>

#include "utils.h"
#include "tests/box_stack_test.h"
#include "tests/fem_cloth_test.h"
#include "tests/polyhedron_test.h"
#include "tests/spring_cloth_test.h"

/// Used for selecting and creating tests.
struct test_creator {
	/// Returns a \ref test_creator for the given test type.
	template <typename Test> [[nodiscard]] inline static test_creator get_creator_for() {
		test_creator result;
		result.name = std::string(Test::get_name());
		result.create = []() {
			return std::make_unique<Test>();
		};
		return result;
	}

	std::string name; ///< The name of this test.
	std::function<std::unique_ptr<test>()> create;///< Creates a test.
};

/// The testbed applicaiton.
class testbed {
public:
	/// Initializes the GLFW window.
	testbed() {
		_wnd = glfwCreateWindow(_width, _height, "PBD Test", nullptr, nullptr);
		glfwSetWindowUserPointer(_wnd, this);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForOpenGL(_wnd, true);
		ImGui_ImplOpenGL2_Init();

		glfwSetWindowSizeCallback(_wnd, [](GLFWwindow *wnd, int width, int height) {
			auto &io = ImGui::GetIO();
			io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
			static_cast<testbed*>(glfwGetWindowUserPointer(wnd))->_on_size(width, height);
		});
		glfwSetCursorPosCallback(_wnd, [](GLFWwindow *wnd, double x, double y) {
			auto &io = ImGui::GetIO();
			io.MousePos = ImVec2(static_cast<float>(x), static_cast<float>(y));
			if (!io.WantCaptureMouse) {
				static_cast<testbed*>(glfwGetWindowUserPointer(wnd))->_on_mouse_move(x, y);
			}
		});
		glfwSetMouseButtonCallback(_wnd, [](GLFWwindow *wnd, int button, int action, int mods) {
			auto &io = ImGui::GetIO();
			io.MouseDown[button] = (action == GLFW_PRESS);
			if (!io.WantCaptureMouse) {
				static_cast<testbed*>(glfwGetWindowUserPointer(wnd))->_on_mouse_button(button, action, mods);
			}
		});
		glfwSetScrollCallback(_wnd, [](GLFWwindow *wnd, double xoff, double yoff) {
			auto &io = ImGui::GetIO();
			io.MouseWheel += static_cast<float>(yoff);
			io.MouseWheelH += static_cast<float>(xoff);
			if (!io.WantCaptureMouse) {
				static_cast<testbed*>(glfwGetWindowUserPointer(wnd))->_on_mouse_scroll(xoff, yoff);
			}
		});

		_reset_camera();
	}
	/// No copy construction.
	testbed(const testbed&) = delete;
	/// No copy assignment.
	testbed &operator=(const testbed&) = delete;
	/// Destroys the GLFW window.
	~testbed() {
		glfwDestroyWindow(_wnd);
	}

	/// Renders and handles the GUI.
	void gui() {
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Testbed", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

		if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Wireframe Surfaces", &_draw_options.wireframe_surfaces);
			ImGui::Checkbox("Wireframe Bodies", &_draw_options.wireframe_bodies);
			ImGui::Checkbox("Body Velocity", &_draw_options.body_velocity);
			ImGui::Checkbox("Contacts", &_draw_options.contacts);

			ImGui::Separator();
			ImGui::SliderFloat("Rotation Sensitivity", &_rotation_sensitivity, 0.0f, 0.01f);
			ImGui::SliderFloat("Move Sensitivity", &_move_sensitivity, 0.0f, 0.1f);
			ImGui::SliderFloat("Zoom Sensitivity", &_zoom_sensitivity, 0.0f, 0.01f);
			ImGui::SliderFloat("Scroll Sensitivity", &_scroll_sensitivity, 0.0f, 1.0f);
			if (ImGui::Button("Reset Camera")) {
				_reset_camera();
			}

			if (_gl_error != GL_NO_ERROR) {
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				ImGui::Text("OpenGL Error: %d", static_cast<int>(_gl_error));
				ImGui::PopStyleColor();
			}
		}

		if (ImGui::CollapsingHeader("Simulation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::BeginCombo(
				"Test", _test_index < tests.size() ? tests[_test_index].name.c_str() : "Select Test"
			)) {
				for (std::size_t i = 0; i < tests.size(); ++i) {
					bool selected = _test_index == i;
					if (ImGui::Selectable(tests[i].name.c_str(), &selected)) {
						_test_index = i;
						_test_running = false;
						_test.reset();
						_test = tests[i].create();
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
				if (_test_index < tests.size()) {
					_test = tests[_test_index].create();
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
			ImGui::SliderFloat("Maximum Frame Time", &_max_frametime, 0.0, 1.0);
			if (_update_truncated) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			}
			if (_time_scale < 100.0) {
				ImGui::LabelText(
					"Simulation Speed", "%5.1f%% x %.1f%% = %5.1f%%",
					_simulation_speed * 100.0, _time_scale, _simulation_speed * _time_scale
				);
			} else {
				ImGui::LabelText("Simulation Speed", "%5.1f%%", _simulation_speed * 100.0);
			}
			if (_update_truncated) {
				ImGui::PopStyleColor();
			}

			ImGui::SliderFloat(
				"RA Timestep Factor", &_timestep_cost_factor, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic
			);
			ImGui::LabelText("RA Timestep Cost", "%.3fms", _timestep_cost);
		}

		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
	}

	/// Renders all objects.
	void render() {
		glMatrixMode(GL_PROJECTION);
		debug_render::set_matrix(_camera.projection_view_matrix);

		glMatrixMode(GL_MODELVIEW);
		if (_test) {
			_test->render(_draw_options);
		}

		if (_mouse_buttons[GLFW_MOUSE_BUTTON_LEFT] || _mouse_buttons[GLFW_MOUSE_BUTTON_MIDDLE]) {
			pbd::cvec3d center = _camera_params.look_at;
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

		_gl_error = glGetError();
	}

	/// Updates the simulation.
	void update() {
		if (_test_running) {
			auto now = std::chrono::high_resolution_clock::now();
			double dt = std::chrono::duration<double>(now - _last_update).count();
			if (_test) {
				_update_truncated = false;
				double target = dt * (_time_scale / 100.0), consumed = 0.0;
				_time_accum += target;
				auto timestep_beg = std::chrono::high_resolution_clock::now();
				for (; _time_accum >= _time_step; _time_accum -= _time_step) {
					_test->timestep(_time_step, _iters);
					consumed += _time_step;
					
					auto timestep_end = std::chrono::high_resolution_clock::now();
					
					double this_cost = std::chrono::duration<double, std::milli>(timestep_end - timestep_beg).count();
					_timestep_cost = (1.0f - _timestep_cost_factor) * _timestep_cost + _timestep_cost_factor * this_cost;
					timestep_beg = timestep_end;

					auto frame = std::chrono::duration<double>(timestep_end - now).count();
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

	/// The main loop body.
	[[nodiscard]] bool loop() {
		if (glfwWindowShouldClose(_wnd)) {
			return false;
		}

		if (_test) {
			_test->camera_params = _camera_params;
			_test->camera = _camera;
		}

		update();

		glfwMakeContextCurrent(_wnd);
		glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		render();
		gui();

		glfwSwapBuffers(_wnd);
		return true;
	}

	std::vector<test_creator> tests; ///< The list of tests.
protected:
	GLFWwindow *_wnd = nullptr; ///< The GLFW window.
	int
		_width = 800, ///< The width of this window.
		_height = 600; ///< The height of this window.

	std::unique_ptr<test> _test; ///< The currently active test.
	std::size_t _test_index = std::numeric_limits<std::size_t>::max(); ///< The test that's currently selected.
	std::chrono::high_resolution_clock::time_point _last_update; ///< The time when the simulation was last updated.
	double _time_accum = 0.0; ///< Accumulated time.

	bool _test_running = false; ///< Whether the test is currently running.
	float _time_scale = 100.0f; ///< Time scaling.
	float _time_step = 0.001f; ///< Time step.
	int _iters = 1; ///< Solver iterations.

	float _max_frametime = 0.1f; ///< Maximum frame time.
	double _simulation_speed = 0.0f; ///< Simulation speed.
	bool _update_truncated = false; ///< Whether the update was terminated early to prevent locking up.
	double _timestep_cost = 0.0f; ///< Running average of timestep costs.
	float _timestep_cost_factor = 0.01f; ///< Running average factor of timestep costs.

	draw_options _draw_options; ///< Whether the surfaces are displayed as wireframes.
	float _rotation_sensitivity = 0.005f; ///< Rotation sensitivity.
	float _move_sensitivity = 0.05f; ///< Move sensitivity.
	float _zoom_sensitivity = 0.001f; ///< Zoom sensitivity.
	/// Sensitivity for scrolling to move the camera closer and further from the focus point.
	float _scroll_sensitivity = 0.95f;
	pbd::camera_parameters _camera_params = pbd::uninitialized; ///< Camera parameters.
	pbd::camera _camera = pbd::uninitialized; ///< Camera.
	GLenum _gl_error = GL_NO_ERROR; ///< OpenGL error.

	pbd::cvec2d _prev_mouse_position = pbd::uninitialized; ///< Last mouse position.
	bool _mouse_buttons[8]{}; ///< State of all mouse buttons.

	/// Size changed callback.
	void _on_size(int w, int h) {
		_width = w;
		_height = h;

		// update viewport
		glfwMakeContextCurrent(_wnd);
		glViewport(0, 0, _width, _height);

		_camera_params.aspect_ratio = _width / static_cast<double>(_height);
		_camera = pbd::camera::from_parameters(_camera_params);
	}
	/// Resets \ref _camera_parameters and \ref _camera.
	void _reset_camera() {
		_camera_params = pbd::camera_parameters::create_look_at(pbd::zero, { 3.0, 4.0, 5.0 });
		_on_size(_width, _height);
	}

	/// Mouse movement callback.
	void _on_mouse_move(double x, double y) {
		bool camera_changed = false;

		pbd::cvec2d new_position = { x, y };
		auto offset = new_position - _prev_mouse_position;
		if (_mouse_buttons[GLFW_MOUSE_BUTTON_MIDDLE]) {
			auto offset3 = _move_sensitivity * (_camera.unit_up * offset[1] - _camera.unit_right * offset[0]);
			_camera_params.look_at += offset3;
			_camera_params.position += offset3;
			camera_changed = true;
		}
		if (_mouse_buttons[GLFW_MOUSE_BUTTON_LEFT]) {
			auto offset3 = _camera_params.position - _camera_params.look_at;
			offset3 = pbd::quat::from_normalized_axis_angle(
				_camera.unit_right, -_rotation_sensitivity * offset[1]
			).rotate(offset3);
			offset3 = pbd::quat::from_normalized_axis_angle(
				_camera_params.world_up, -_rotation_sensitivity * offset[0]
			).rotate(offset3);
			_camera_params.position = _camera_params.look_at + offset3;
			camera_changed = true;
		}
		if (_mouse_buttons[GLFW_MOUSE_BUTTON_RIGHT]) {
			_camera_params.fov_y_radians += _zoom_sensitivity * offset[1];
			camera_changed = true;
		}
		_prev_mouse_position = new_position;

		if (camera_changed) {
			_camera = pbd::camera::from_parameters(_camera_params);
		}
	}
	/// Mouse button callback.
	void _on_mouse_button(int button, int action, [[maybe_unused]] int mods) {
		if (action == GLFW_PRESS) {
			if (mods & GLFW_MOD_ALT) {
				_mouse_buttons[button] = true;
			}
		} else {
			_mouse_buttons[button] = false;
		}
	}
	/// Mouse scroll callback.
	void _on_mouse_scroll([[maybe_unused]] double xoff, double yoff) {
		pbd::cvec3d diff = _camera_params.position - _camera_params.look_at;
		diff *= std::pow(_scroll_sensitivity, yoff);
		_camera_params.position = _camera_params.look_at + diff;
		_camera = pbd::camera::from_parameters(_camera_params);
	}
};

int main() {
	if (!glfwInit()) {
		return -1;
	}

	{
		testbed tb;
		tb.tests.emplace_back(test_creator::get_creator_for<convex_hull_test>());
		tb.tests.emplace_back(test_creator::get_creator_for<fem_cloth_test>());
		tb.tests.emplace_back(test_creator::get_creator_for<spring_cloth_test>());
		tb.tests.emplace_back(test_creator::get_creator_for<box_stack_test>());
		while (tb.loop()) {
			glfwPollEvents();
		}
	}

	glfwTerminate();
	return 0;
}
