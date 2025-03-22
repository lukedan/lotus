#pragma once

#include <random>

#include <lotus/algorithms/convex_hull.h>

#include "test.h"
#include "../utils.h"

class convex_hull_test : public test {
public:
	using hull = lotus::incremental_convex_hull;

	struct empty {
		empty(lotus::uninitialized_t) {
		}
	};
	using random_engine = std::default_random_engine;
	static constexpr int max_verts = 1000;

	explicit convex_hull_test(const test_context &tctx) :
		test(tctx),
		_convex_hull_storage(hull::create_storage_for_num_vertices(max_verts)) {

		soft_reset();
	}
	~convex_hull_test() override {
		_convex_hull = nullptr;
	}

	void timestep(scalar, u32) override {
		if (_cur_vertex < _vertices.size()) {
			_convex_hull.add_vertex(_vertices[_cur_vertex]);
			++_cur_vertex;
			_update_properties();
		}
	}

	void soft_reset() override {
		_render = debug_render();
		_render.ctx = &_get_test_context();

		_vertices.clear();
		_vertex_states.clear();
		std::normal_distribution dist(0.0f, 1.0f);
		std::default_random_engine eng(static_cast<std::default_random_engine::result_type>(_seed));
		for (int i = 0; i < _num_vertices; ++i) {
			vec3 v = lotus::uninitialized;
			v[0] = dist(eng);
			v[1] = dist(eng);
			v[2] = dist(eng);
			_vertices.emplace_back(v);
		}

		_convex_hull = nullptr;
		_convex_hull = _convex_hull_storage.create_state_for_tetrahedron({
			_vertices[0], _vertices[1], _vertices[2], _vertices[3]
		});
		_cur_vertex = 4;
		_update_properties();
	}

	void render(
		lotus::renderer::context &ctx,
		lotus::renderer::context::queue &q,
		lotus::renderer::constant_uploader &uploader,
		lotus::renderer::image2d_color color,
		lotus::renderer::image2d_depth_stencil ds,
		lotus::cvec2u32 size
	) override {
		const auto [poly_verts, poly_tris] = _get_polyhedron();
		std::vector<bool> poly_vert_used(poly_verts.size(), false);
		std::vector<vec3> verts;
		std::vector<vec3> normals;
		std::vector<u32> indices;
		for (const std::array<u32, 3> &tri : poly_tris) {
			const vec3 p0 = poly_verts[tri[0]];
			const vec3 p1 = poly_verts[tri[1]];
			const vec3 p2 = poly_verts[tri[2]];
			const vec3 n = lotus::vec::unsafe_normalize(lotus::vec::cross(p1 - p0, p2 - p0));

			indices.emplace_back(verts.size());
			verts.emplace_back(p0);
			normals.emplace_back(n);

			indices.emplace_back(verts.size());
			verts.emplace_back(p1);
			normals.emplace_back(n);

			indices.emplace_back(verts.size());
			verts.emplace_back(p2);
			normals.emplace_back(n);

			poly_vert_used[tri[0]] = true;
			poly_vert_used[tri[1]] = true;
			poly_vert_used[tri[2]] = true;
		}
		_render.draw_body(
			verts,
			normals,
			indices,
			mat44s::identity(),
			lotus::linear_rgba_f(1.0f, 1.0f, 1.0f, 1.0f),
			_get_test_context().wireframe_bodies
		);

		for (usize i = 0; i < poly_verts.size(); ++i) {
			const vec3 p = poly_verts[i];
			const float size = 0.1f;
			const mat44s trans({
				{ size, 0.0f, 0.0f, p[0] },
				{ 0.0f, size, 0.0f, p[1] },
				{ 0.0f, 0.0f, size, p[2] },
				{ 0.0f, 0.0f, 0.0f, 1.0f },
			});
			_render.draw_sphere(
				trans,
				poly_vert_used[i] ? lotus::linear_rgba_f(1.0f, 0.0f, 0.0f, 1.0f) : lotus::linear_rgba_f(0.4f, 0.4f, 0.4f, 1.0f),
				false
			);
		}

		_render.flush(ctx, q, uploader, color, ds, size);
		/*glPointSize(10.0f);
		glDisable(GL_LIGHTING);
		glBegin(GL_POINTS);
		for (usize i = 0; i < _vertices.size(); ++i) {
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
		glEnable(GL_LIGHTING);*/
	}

	void gui() override {
		bool needs_reset = false;

		needs_reset = ImGui::InputInt("Seed", &_seed) || needs_reset;
		needs_reset = ImGui::SliderInt("Vertices", &_num_vertices, 4, max_verts) || needs_reset;
		/*ImGui::LabelText("Faces", "%d", static_cast<int>(_convex_hull.faces.size()));*/
		ImGui::Separator();
		if (needs_reset) {
			soft_reset();
		}

		test::gui();
	}

	inline static std::string get_name() {
		return "Polyhedron Test";
	}
protected:
	lotus::collision::shapes::polyhedron::properties _props = lotus::uninitialized;
	debug_render _render;
	std::vector<vec3> _vertices;
	std::vector<bool> _vertex_states;
	hull::state _convex_hull = nullptr;
	hull::storage<> _convex_hull_storage;
	int _seed = 0;
	int _num_vertices = 100;
	usize _cur_vertex = 0;

	[[nodiscard]] std::pair<std::vector<vec3>, std::vector<std::array<u32, 3>>> _get_polyhedron() const {
		std::vector<vec3> verts;
		std::vector<std::array<u32, 3>> tris;
		for (usize vi = 0; vi < _convex_hull.get_vertex_count(); ++vi) {
			verts.emplace_back(_convex_hull.get_vertex(static_cast<hull::vertex_id>(vi)));
		}
		const hull::face_id any_face = _convex_hull.get_any_face();
		if (any_face != hull::face_id::invalid) {
			hull::face_id cur_face = any_face;
			do {
				const hull::face &face = _convex_hull.get_face(cur_face);
				tris.emplace_back(std::array{
					std::to_underlying(face.vertex_indices[0]),
					std::to_underlying(face.vertex_indices[1]),
					std::to_underlying(face.vertex_indices[2]),
				});
				cur_face = face.next;
			} while (cur_face != any_face);
		}
		return { std::move(verts), std::move(tris) };
	}
	void _update_properties() {
		const auto [verts, tris] = _get_polyhedron();
		_props = lotus::collision::shapes::polyhedron::properties::compute_for(verts, tris);
	}
};
