#include <iostream>
#include <vector>
#include <chrono>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#include <pbd/math/matrix.h>
#include <pbd/math/vector.h>
#include <pbd/math/quaternion.h>
#include <pbd/camera.h>

#include "test.h"
#include "tests/cloth_test.h"

constexpr auto mat1 = pbd::mat34d::identity();
constexpr pbd::mat34d mat2 = pbd::zero;
constexpr pbd::mat34d mat3{
	{1.0, 2.0, 3.0, 4.0},
	{2.0, 3.0, 4.0, 5.0},
	{0.0, 1.0, 2.0, 3.0}
};
constexpr auto vec1 = pbd::cvec3d::create({ 1.0, 2.0, 3.0 });

constexpr auto trans = mat1.transposed();
constexpr auto elem = mat2(2, 1);
constexpr auto row = trans.row(2);
constexpr auto comp = row[2];
constexpr auto col = trans.column(2);
constexpr auto block = trans.block<4, 3>(0, 0);
constexpr auto block2 = trans.block<3, 2>(1, 1);

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
/*constexpr auto quat = pbd::quatd::zero();
constexpr auto quat2 = pbd::quatd::from_wxyz({ 1.0, 2.0, 3.0, 4.0 });
constexpr auto inv_quat2 = quat2.inverse();
constexpr auto quat3 = quat2 + inv_quat2;
constexpr auto quat4 = quat2 - inv_quat2;

constexpr auto quat_w = quat.w();
constexpr auto quat_mag = quat.squared_magnitude();*/

/// A mesh.
struct mesh {
	std::vector<pbd::cvec3d> vertices; ///< Vertices of the mesh.
	std::vector<pbd::cvec3d> normals; ///< Normals of the mesh.
	std::vector<int> indices; ///< Triangle indices.
};

/// Returns the vertices on a sphere.
mesh get_sphere(std::size_t divisions) {
	return mesh();
}

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

		_test = std::make_unique<cloth_test>();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
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

		if (ImGui::Button("Reset Camera")) {
			_reset_camera();
		}
		ImGui::Separator();

		if (ImGui::Checkbox("Test Running", &_test_running)) {
			if (_test_running) {
				_last_update = std::chrono::high_resolution_clock::now();
			}
		}
		ImGui::SliderFloat("Time Scaling", &_time_scale, 0.0f, 1.0f);
		ImGui::SliderFloat("Time Step", &_time_step, 0.001f, 0.1f, "%.3f", ImGuiSliderFlags_Logarithmic);
		if (ImGui::Button("Reset Test")) {
			_test = std::make_unique<cloth_test>();
			_test_running = false;
		}

		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
	}

	/// Renders all objects.
	void render() {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		auto &mat = _camera.projection_view_matrix;
		GLdouble values[16]{
			mat(0, 0), mat(1, 0), mat(2, 0), mat(3, 0),
			mat(0, 1), mat(1, 1), mat(2, 1), mat(3, 1),
			mat(0, 2), mat(1, 2), mat(2, 2), mat(3, 2),
			mat(0, 3), mat(1, 3), mat(2, 3), mat(3, 3)
		};
		glLoadMatrixd(values);

		glDisable(GL_CULL_FACE);

		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
		float lightdir[4]{ 0.3, 0.4, 0.5, 0.0 };
		glLightfv(GL_LIGHT0, GL_POSITION, lightdir);

		if (_test) {
			_test->render();
		}

		// draw ground
		glColor4d(1.0, 1.0, 0.8, 0.5);
		glBegin(GL_TRIANGLE_STRIP);
		glNormal3d(0.0, 0.0, 1.0);
		glVertex3d(-10.0, -10.0, 0.0);
		glVertex3d(10.0, -10.0, 0.0);
		glVertex3d(-10.0, 10.0, 0.0);
		glVertex3d(10.0, 10.0, 0.0);
		glEnd();
	}

	/// The main loop body.
	[[nodiscard]] bool loop() {
		if (glfwWindowShouldClose(_wnd)) {
			return false;
		}
		glfwMakeContextCurrent(_wnd);
		glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (_test_running) {
			auto now = std::chrono::high_resolution_clock::now();
			double dt = std::chrono::duration<double>(now - _last_update).count();
			if (_test) {
				_test->update(dt * _time_scale, _time_step);
			}
			_last_update = now;
		}
		render();
		gui();

		glfwSwapBuffers(_wnd);
		return true;
	}
protected:
	GLFWwindow *_wnd = nullptr; ///< The GLFW window.
	int
		_width = 800, ///< The width of this window.
		_height = 600; ///< The height of this window.

	std::unique_ptr<test> _test; ///< The currently active test.
	std::chrono::high_resolution_clock::time_point _last_update; ///< The time when the simulation was last updated.
	bool _test_running = false; ///< Whether the test is currently running.
	float _time_scale = 1.0f; ///< Time scaling.
	float _time_step = 0.001f; ///< Time step.

	pbd::camera_parameters _camera_params = pbd::uninitialized; ///< Camera parameters.
	pbd::camera _camera = pbd::uninitialized; ///< Camera.

	pbd::cvec2d_t _prev_mouse_position = pbd::uninitialized; ///< Last mouse position.
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
		_camera_params = pbd::camera_parameters::create_look_at(pbd::zero, pbd::cvec3d::create({ 6.0, 8.0, 10.0 }));
		_on_size(_width, _height);
	}

	/// Mouse movement callback.
	void _on_mouse_move(double x, double y) {
		bool camera_changed = false;

		auto new_position = pbd::cvec2d::create({ x, y });
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
		while (tb.loop()) {
			glfwPollEvents();
		}
	}

	glfwTerminate();
	return 0;
}
