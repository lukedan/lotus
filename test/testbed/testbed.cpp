#include <iostream>
#include <vector>
#include <chrono>
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
#include "tests/spring_cloth_test.h"
#include "tests/fem_cloth_test.h"

constexpr auto mat1 = pbd::mat34d::identity();
constexpr pbd::mat34d mat2 = pbd::zero;
constexpr pbd::mat34d mat3{
	{1.0, 2.0, 3.0, 4.0},
	{2.0, 3.0, 4.0, 5.0},
	{0.0, 1.0, 2.0, 3.0}
};
constexpr pbd::cvec3d vec1 = { 1.0, 2.0, 3.0 };

constexpr auto trans = mat1.transposed();
constexpr auto elem = mat2(2, 1);
constexpr auto row = trans.row(2);
constexpr auto comp = row[2];
constexpr auto col = trans.column(2);
constexpr auto block = trans.block<4, 3>(0, 0);
constexpr auto block2 = trans.block<3, 2>(1, 1);

constexpr auto rows = pbd::matd::concat_rows(vec1.transposed(), block.row(1), row);
constexpr auto cols = pbd::matd::concat_columns(vec1, mat3.column(1), row.transposed());

constexpr auto mul = mat3 * trans;
constexpr auto add = mat3 + mat1;
constexpr auto sub = mat3 - mat1;
constexpr auto scale1 = mat3 * 3.0;
constexpr auto scale2 = 3.0 * mat3;
constexpr auto scale3 = mat3 / 3.0;
constexpr auto neg = -mat3;

constexpr auto sqr_norm = vec1.squared_norm();
constexpr auto dot_prod = pbd::vec::dot(vec1, trans.row(1).transposed());

// doesn't work on MSVC
/*constexpr auto lu_decomp = pbd::matd::lup_decompose(pbd::mat33d({
	{2.0, -1.0, -2.0},
	{-4.0, 6.0, 3.0},
	{-4.0, -2.0, 8.0}
}));

constexpr auto quat = pbd::quatd::zero();
constexpr auto quat2 = pbd::quatd::from_wxyz(1.0, 2.0, 3.0, 4.0);
constexpr auto inv_quat2 = quat2.inverse();
constexpr auto quat3 = quat2 + inv_quat2;
constexpr auto quat4 = quat2 - inv_quat2;

constexpr auto quat_w = quat.w();
constexpr auto quat_mag = quat.squared_magnitude();*/


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
			io.DisplaySize = ImVec2(width, height);
			static_cast<testbed*>(glfwGetWindowUserPointer(wnd))->_on_size(width, height);
		});
		glfwSetCursorPosCallback(_wnd, [](GLFWwindow *wnd, double x, double y) {
			auto &io = ImGui::GetIO();
			io.MousePos = ImVec2(x, y);
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
			io.MouseWheel += yoff;
			io.MouseWheelH += xoff;
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
		ImGui::Begin("Testbed");

		if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Button("Reset Camera")) {
				_reset_camera();
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

		if (ImGui::CollapsingHeader("Simulation Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
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
		}

		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
	}

	/// Renders all objects.
	void render() {
		glDisable(GL_CULL_FACE);
		glEnable(GL_NORMALIZE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);

		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
		float lightdir[4]{ 0.3, 0.4, 0.5, 0.0 };
		glLightfv(GL_LIGHT0, GL_POSITION, lightdir);

		glMatrixMode(GL_PROJECTION);
		set_matrix(_camera.projection_view_matrix);
		glMatrixMode(GL_MODELVIEW);

		if (_test) {
			_test->render();
		}

		// draw ground
		glLoadIdentity();
		glColor4d(1.0, 1.0, 0.8, 0.5);
		glBegin(GL_TRIANGLE_STRIP);
		glNormal3d(0.0, 0.0, 1.0);
		glVertex3d(-10.0, -10.0, 0.0);
		glVertex3d(10.0, -10.0, 0.0);
		glVertex3d(-10.0, 10.0, 0.0);
		glVertex3d(10.0, 10.0, 0.0);
		glEnd();
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
				for (; _time_accum >= _time_step; _time_accum -= _time_step, consumed += _time_step) {
					_test->timestep(_time_step, _iters);
					auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::high_resolution_clock::now() - now
						).count();
					if (ms > 100) {
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

	double _simulation_speed = 0.0f; ///< Simulation speed.
	bool _update_truncated = false; ///< Whether the update was terminated early to prevent locking up.

	pbd::camera_parameters _camera_params = pbd::uninitialized; ///< Camera parameters.
	pbd::camera _camera = pbd::uninitialized; ///< Camera.

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
		_camera_params = pbd::camera_parameters::create_look_at(pbd::zero, { 6.0, 8.0, 10.0 });
		_on_size(_width, _height);
	}

	/// Mouse movement callback.
	void _on_mouse_move(double x, double y) {
		bool camera_changed = false;

		pbd::cvec2d new_position = { x, y };
		auto offset = new_position - _prev_mouse_position;
		if (_mouse_buttons[GLFW_MOUSE_BUTTON_MIDDLE]) {
			auto offset3 = 0.1 * (_camera.unit_up * offset[1] - _camera.unit_right * offset[0]);
			_camera_params.look_at += offset3;
			_camera_params.position += offset3;
			camera_changed = true;
		}
		if (_mouse_buttons[GLFW_MOUSE_BUTTON_LEFT]) {
			auto offset3 = _camera_params.position - _camera_params.look_at;
			offset3 = pbd::quatd::from_normalized_axis_angle(_camera.unit_right, -0.005 * offset[1]).rotate(offset3);
			offset3 = pbd::quatd::from_normalized_axis_angle(_camera_params.world_up, -0.005 * offset[0]).rotate(offset3);
			_camera_params.position = _camera_params.look_at + offset3;
			camera_changed = true;
		}
		if (_mouse_buttons[GLFW_MOUSE_BUTTON_RIGHT]) {
			_camera_params.fov_y_radians += 0.001 * offset[1];
			camera_changed = true;
		}
		_prev_mouse_position = new_position;

		if (camera_changed) {
			_camera = pbd::camera::from_parameters(_camera_params);
		}
	}
	/// Mouse button callback.
	void _on_mouse_button(int button, int action, int mods) {
		_mouse_buttons[button] = action == GLFW_PRESS;
	}
	/// Mouse scroll callback.
	void _on_mouse_scroll(double xoff, double yoff) {
		_camera_params.position += _camera.unit_forward * yoff;
		_camera = pbd::camera::from_parameters(_camera_params);
	}
};

int main() {
	if (!glfwInit()) {
		return -1;
	}

	{
		testbed tb;
		tb.tests.emplace_back(test_creator::get_creator_for<spring_cloth_test>());
		tb.tests.emplace_back(test_creator::get_creator_for<fem_cloth_test>());
		while (tb.loop()) {
			glfwPollEvents();
		}
	}

	glfwTerminate();
	return 0;
}
