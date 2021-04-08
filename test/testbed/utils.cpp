#include "utils.h"

#include <vector>

#include <GLFW/glfw3.h>

#include <pbd/math/constants.h>

void debug_render::set_matrix(pbd::mat44d mat) {
	GLdouble values[16]{
		mat(0, 0), mat(1, 0), mat(2, 0), mat(3, 0),
		mat(0, 1), mat(1, 1), mat(2, 1), mat(3, 1),
		mat(0, 2), mat(1, 2), mat(2, 2), mat(3, 2),
		mat(0, 3), mat(1, 3), mat(2, 3), mat(3, 3)
	};
	glLoadMatrixd(values);
}

void debug_render::set_color(colorf color) {
	glColor4f(color[0], color[1], color[2], color[3]);
}

void debug_render::draw_sphere() {
	constexpr std::size_t _z_slices = 10;
	constexpr std::size_t _xy_slices = 30;
	constexpr double _z_slice_angle = pbd::pi / _z_slices;
	constexpr double _xy_slice_angle = 2.0 * pbd::pi / _xy_slices;

	static std::vector<pbd::cvec3d> _vertices;
	static std::vector<std::size_t> _indices;

	if (_vertices.empty()) {
		// http://www.songho.ca/opengl/gl_sphere.html
		for (std::size_t i = 0; i <= _z_slices; ++i) {
			double z_angle = pbd::pi / 2 - i * _z_slice_angle;
			double xy = 0.5 * std::cos(z_angle);
			double z = 0.5 * std::sin(z_angle);

			for (std::size_t j = 0; j <= _xy_slices; ++j) {
				double xy_angle = j * _xy_slice_angle;

				double x = xy * std::cos(xy_angle);
				double y = xy * std::sin(xy_angle);
				_vertices.emplace_back(x, y, z);
			}
		}

		for (std::size_t i = 0; i < _z_slices; ++i) {
			std::size_t k1 = i * (_xy_slices + 1);
			std::size_t k2 = k1 + _xy_slices + 1;

			for (std::size_t j = 0; j < _xy_slices; ++j, ++k1, ++k2) {
				if (i != 0) {
					_indices.push_back(k1);
					_indices.push_back(k2);
					_indices.push_back(k1 + 1);
				}
				if (i != (_z_slices - 1)) {
					_indices.push_back(k1 + 1);
					_indices.push_back(k2);
					_indices.push_back(k2 + 1);
				}
			}
		}
	}

	glBegin(GL_TRIANGLES);
	for (std::size_t i = 0; i + 2 < _indices.size(); i += 3) {
		auto p1 = _vertices[_indices[i]];
		auto p2 = _vertices[_indices[i + 1]];
		auto p3 = _vertices[_indices[i + 2]];
		glNormal3d(p1[0], p1[1], p1[2]);
		glVertex3d(p1[0], p1[1], p1[2]);
		glNormal3d(p2[0], p2[1], p2[2]);
		glVertex3d(p2[0], p2[1], p2[2]);
		glNormal3d(p3[0], p3[1], p3[2]);
		glVertex3d(p3[0], p3[1], p3[2]);
	}
	glEnd();
}

void debug_render::draw(bool wf_surf) const {
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
	float lightdir[4]{ 0.3f, 0.4f, 0.5f, 0.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, lightdir);

	for (std::size_t i = 0; i < engine->bodies.size(); ++i) {
		const auto &body = engine->bodies[i];

		set_color(i < object_colors.size() ? object_colors[i] : colorf(1.0f, 1.0f, 1.0f, 1.0f));

		auto mat = pbd::mat44d::identity();
		mat.set_block(0, 0, body.state.rotation.into_matrix());
		mat.set_block(0, 3, body.state.position);
		set_matrix(mat);

		std::visit(
			[&](const auto &shape) {
				draw_body(shape);
			},
			body.body_shape.value
		);
	}

	glLoadIdentity();
	if (wf_surf) {
		glDisable(GL_LIGHTING);
	}
	std::vector<pbd::cvec3d> normals(engine->particles.size(), pbd::uninitialized);
	for (const auto &surface : surfaces) {
		// compute normals
		std::fill(normals.begin(), normals.end(), pbd::zero);
		for (const auto &tri : surface.triangles) {
			auto p1 = engine->particles[tri[0]].state.position;
			auto p2 = engine->particles[tri[1]].state.position;
			auto p3 = engine->particles[tri[2]].state.position;
			auto diff = pbd::vec::cross(p2 - p1, p3 - p1);
			normals[tri[0]] += diff;
			normals[tri[1]] += diff;
			normals[tri[2]] += diff;
		}

		set_color(surface.color);
		if (wf_surf) {
			for (const auto &tri : surface.triangles) {
				auto p1 = engine->particles[tri[0]].state.position;
				auto p2 = engine->particles[tri[1]].state.position;
				auto p3 = engine->particles[tri[2]].state.position;
				glBegin(GL_LINE_LOOP);
				glVertex3d(p1[0], p1[1], p1[2]);
				glVertex3d(p2[0], p2[1], p2[2]);
				glVertex3d(p3[0], p3[1], p3[2]);
				glEnd();
			}
		} else {
			glBegin(GL_TRIANGLES);
			for (const auto &tri : surface.triangles) {
				auto n1 = normals[tri[0]];
				auto n2 = normals[tri[1]];
				auto n3 = normals[tri[2]];
				auto p1 = engine->particles[tri[0]].state.position;
				auto p2 = engine->particles[tri[1]].state.position;
				auto p3 = engine->particles[tri[2]].state.position;
				glNormal3d(n1[0], n1[1], n1[2]);
				glVertex3d(p1[0], p1[1], p1[2]);
				glNormal3d(n2[0], n2[1], n2[2]);
				glVertex3d(p2[0], p2[1], p2[2]);
				glNormal3d(n3[0], n3[1], n3[2]);
				glVertex3d(p3[0], p3[1], p3[2]);
			}
			glEnd();
		}
	}
}

void debug_render::draw_body(const pbd::shapes::plane&) {
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(0.0f, 0.0f, 1.0f);
	
	glVertex3f(-100.0f, -100.0f, 0.0f);
	glVertex3f(100.0f, -100.0f, 0.0f);
	glVertex3f(-100.0f, 100.0f, 0.0f);
	glVertex3f(100.0f, 100.0f, 0.0f);

	glEnd();
}

void debug_render::draw_body(const pbd::shapes::sphere &sphere) {
	glPushMatrix();
	glScaled(2.0 * sphere.radius, 2.0 * sphere.radius, 2.0 * sphere.radius);
	draw_sphere();
	glPopMatrix();
}
