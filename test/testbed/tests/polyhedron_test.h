#pragma once

#include <random>

#include <pbd/algorithms/convex_hull.h>

#include "test.h"
#include "../utils.h"

class convex_hull_test : public test {
public:
	struct empty {
		empty(pbd::uninitialized_t) {
		}
	};
	using random_engine = std::default_random_engine;
	using convex_hull_t = pbd::incremental_convex_hull<empty, empty>;

	convex_hull_test() {
		soft_reset();
	}

	void timestep(double, std::size_t) override {
		if (_cur_vertex < _vertices.size()) {
			_convex_hull.add_vertex(
				convex_hull_t::vertex::create(_vertices[_cur_vertex], pbd::uninitialized), _dummy_face_data
			);
			++_cur_vertex;
			_update_properties();
		}
	}

	void soft_reset() override {
		_vertices.clear();
		_vertex_states.clear();
		std::uniform_real_distribution dist(-1.0, 1.0);
		std::default_random_engine eng(_seed);
		for (int i = 0; i < _num_vertices; ++i) {
			pbd::cvec3d v = pbd::uninitialized;
			v[0] = dist(eng);
			v[1] = dist(eng);
			v[2] = dist(eng);
			_vertices.emplace_back(v);
		}

		_convex_hull = convex_hull_t::for_tetrahedron({
			convex_hull_t::vertex::create(_vertices[0], pbd::uninitialized),
			convex_hull_t::vertex::create(_vertices[1], pbd::uninitialized),
			convex_hull_t::vertex::create(_vertices[2], pbd::uninitialized),
			convex_hull_t::vertex::create(_vertices[3], pbd::uninitialized)
		}, _dummy_face_data);
		_cur_vertex = 4;
		_update_properties();
	}

	void render(const draw_options &options) override {
		debug_render::setup_draw();

		glPointSize(10.0f);
		glDisable(GL_LIGHTING);
		glBegin(GL_POINTS);
		for (std::size_t i = 0; i < _vertices.size(); ++i) {
			std::array<float, 3> color{ 0.5f, 0.5f, 0.5f };
			if (i == _cur_vertex) {
				color = { 1.0f, 1.0f, 0.0f };
			} else if (i < _cur_vertex) {
				color = { 1.0f, 1.0f, 1.0f };
			}
			glColor3f(color[0], color[1], color[2]);
			glVertex3d(_vertices[i][0], _vertices[i][1], _vertices[i][2]);
		}
		glEnd();
		glEnable(GL_LIGHTING);

		if (options.wireframe_bodies) {
			glDisable(GL_LIGHTING);
		}
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		for (const auto &tri : _convex_hull.faces) {
			if (options.wireframe_bodies) {
				glBegin(GL_LINE_LOOP);
			} else {
				glBegin(GL_TRIANGLES);
			}
			auto p1 = _convex_hull.vertices[tri.vertex_indices[0]].position;
			auto p2 = _convex_hull.vertices[tri.vertex_indices[1]].position;
			auto p3 = _convex_hull.vertices[tri.vertex_indices[2]].position;
			glNormal3d(tri.normal[0], tri.normal[1], tri.normal[2]);
			glVertex3d(p1[0], p1[1], p1[2]);
			glVertex3d(p2[0], p2[1], p2[2]);
			glVertex3d(p3[0], p3[1], p3[2]);
			glEnd();
		}
		if (options.wireframe_bodies) {
			glEnable(GL_LIGHTING);
		}

		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		glBegin(GL_POINTS);
		glColor3f(1.0f, 0.0f, 1.0f);
		glVertex3d(_props.center_of_mass[0], _props.center_of_mass[1], _props.center_of_mass[2]);
		glEnd();
		glBegin(GL_LINES);
		auto mat = _props.translated_covariance_matrix(-_props.center_of_mass);
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex3d(_props.center_of_mass[0], _props.center_of_mass[1], _props.center_of_mass[2]);
		glVertex3d(mat(0, 0), mat(1, 0), mat(2, 0));
		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex3d(_props.center_of_mass[0], _props.center_of_mass[1], _props.center_of_mass[2]);
		glVertex3d(mat(0, 1), mat(1, 1), mat(2, 1));
		glColor3f(0.0f, 0.0f, 1.0f);
		glVertex3d(_props.center_of_mass[0], _props.center_of_mass[1], _props.center_of_mass[2]);
		glVertex3d(mat(0, 2), mat(1, 2), mat(2, 2));
		glEnd();
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_LIGHTING);
	}

	void gui() override {
		ImGui::InputInt("Seed", &_seed);
		ImGui::SliderInt("Vertices", &_num_vertices, 4, 1000);
		ImGui::LabelText("Faces", "%d", static_cast<int>(_convex_hull.faces.size()));
		test::gui();
	}

	inline static std::string get_name() {
		return "Polyhedron Test";
	}
protected:
	inline static void _dummy_face_data(const convex_hull_t&, const convex_hull_t::face&) {
	}

	pbd::shapes::polyhedron::properties _props = pbd::uninitialized;
	std::vector<pbd::cvec3d> _vertices;
	std::vector<bool> _vertex_states;
	convex_hull_t _convex_hull;
	int _seed = 0;
	int _num_vertices = 100;
	std::size_t _cur_vertex = 0;

	void _update_properties() {
		std::vector<pbd::cvec3d> verts;
		std::vector<std::array<std::size_t, 3>> tris;
		for (const auto &v : _convex_hull.vertices) {
			verts.emplace_back(v.position);
		}
		for (const auto &tri : _convex_hull.faces) {
			tris.emplace_back(tri.vertex_indices);
		}
		_props = pbd::shapes::polyhedron::properties::compute_for(verts, tris);
	}
};
