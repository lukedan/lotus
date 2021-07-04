#include "utils.h"

#include <vector>

#include <GLFW/glfw3.h>

#include <lotus/math/constants.h>

void debug_render::set_matrix(lotus::mat44d mat) {
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

void debug_render::setup_draw() {
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

	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
}

void debug_render::draw_sphere() {
	constexpr std::size_t _z_slices = 10;
	constexpr std::size_t _xy_slices = 30;
	constexpr double _z_slice_angle = lotus::pi / _z_slices;
	constexpr double _xy_slice_angle = 2.0 * lotus::pi / _xy_slices;

	static std::vector<lotus::cvec3d> _vertices;
	static std::vector<std::size_t> _indices;

	if (_vertices.empty()) {
		// http://www.songho.ca/opengl/gl_sphere.html
		for (std::size_t i = 0; i <= _z_slices; ++i) {
			double z_angle = lotus::pi / 2 - static_cast<double>(i) * _z_slice_angle;
			double xy = 0.5 * std::cos(z_angle);
			double z = 0.5 * std::sin(z_angle);

			for (std::size_t j = 0; j <= _xy_slices; ++j) {
				double xy_angle = static_cast<double>(j) * _xy_slice_angle;

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

void debug_render::draw_sphere_wireframe() {
	constexpr std::size_t _slices = 30;
	constexpr double _slice_angle = 2.0 * lotus::pi / _slices;

	static std::vector<lotus::cvec2d> _loop;

	if (_loop.empty()) {
		for (std::size_t i = 0; i < _slices; ++i) {
			double angle = static_cast<double>(i) * _slice_angle;
			_loop.emplace_back(0.5 * std::cos(angle), 0.5 * std::sin(angle));
		}
	}

	glBegin(GL_LINE_LOOP);
	for (auto p : _loop) {
		glVertex3d(p[0], p[1], 0.0);
	}
	glEnd();

	glBegin(GL_LINE_LOOP);
	for (auto p : _loop) {
		glVertex3d(p[0], 0.0, p[1]);
	}
	glEnd();

	glBegin(GL_LINE_LOOP);
	for (auto p : _loop) {
		glVertex3d(0.0, p[0], p[1]);
	}
	glEnd();
}

void debug_render::draw(draw_options opt) const {
	setup_draw();

	glMatrixMode(GL_MODELVIEW);

	if (opt.wireframe_bodies) {
		glDisable(GL_LIGHTING);
	}
	for (const lotus::physics::body &b : engine->bodies) {
		const body_visual *visual = nullptr;
		if (b.user_data) {
			visual = static_cast<const body_visual*>(b.user_data);
		}

		if (visual) {
			set_color(visual->color);
		} else {
			set_color({ 1.0f, 1.0f, 1.0f, 1.0f });
		}
		auto mat = lotus::mat44d::identity();
		mat.set_block(0, 0, b.state.rotation.into_matrix());
		mat.set_block(0, 3, b.state.position);
		set_matrix(mat);

		std::visit(
			[&](const auto &shape) {
				draw_body(shape, visual, opt.wireframe_bodies);
			},
			b.body_shape->value
		);
	}
	if (opt.wireframe_bodies) {
		glEnable(GL_LIGHTING);
	}

	// surfaces
	glLoadIdentity();
	if (opt.wireframe_surfaces) {
		glDisable(GL_LIGHTING);
	}
	std::vector<lotus::cvec3d> normals(engine->particles.size(), lotus::uninitialized);
	for (const auto &surface : surfaces) {
		// compute normals
		std::fill(normals.begin(), normals.end(), lotus::zero);
		for (const auto &tri : surface.triangles) {
			auto p1 = engine->particles[tri[0]].state.position;
			auto p2 = engine->particles[tri[1]].state.position;
			auto p3 = engine->particles[tri[2]].state.position;
			auto diff = lotus::vec::cross(p2 - p1, p3 - p1);
			normals[tri[0]] += diff;
			normals[tri[1]] += diff;
			normals[tri[2]] += diff;
		}

		set_color(surface.color);
		if (opt.wireframe_surfaces) {
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
	if (opt.wireframe_surfaces) {
		glEnable(GL_LIGHTING);
	}

	// debug stuff
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glPointSize(5.0f);

	if (opt.body_velocity) {
		glPushMatrix();
		glLoadIdentity();
		glBegin(GL_LINES);
		for (const lotus::physics::body &b : engine->bodies) {
			glColor3f(1.0f, 0.0f, 0.0f);
			auto p1 = b.state.position + b.state.linear_velocity;
			glVertex3d(b.state.position[0], b.state.position[1], b.state.position[2]);
			glVertex3d(p1[0], p1[1], p1[2]);

			glColor3f(0.0f, 1.0f, 0.0f);
			auto p2 = b.state.position + b.state.angular_velocity;
			glVertex3d(b.state.position[0], b.state.position[1], b.state.position[2]);
			glVertex3d(p2[0], p2[1], p2[2]);
		}
		glEnd();
		glPopMatrix();
	}

	if (opt.contacts) {
		glColor3f(0.0f, 0.0f, 1.0f);
		glBegin(GL_POINTS);
		for (const auto &c : engine->contact_constraints) {
			auto p1 = c.body1->state.position + c.body1->state.rotation.rotate(c.offset1);
			auto p2 = c.body2->state.position + c.body2->state.rotation.rotate(c.offset2);
			glVertex3d(p1[0], p1[1], p1[2]);
			glVertex3d(p2[0], p2[1], p2[2]);
		}
		glEnd();
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
}

void debug_render::draw_body(const lotus::collision::shapes::plane&, const body_visual*, bool wireframe) {
	if (wireframe) {
		glBegin(GL_LINES);
		for (int x = -100; x <= 100; ++x) {
			glVertex3f(static_cast<float>(x), -100.0f, 0.0f);
			glVertex3f(static_cast<float>(x), 100.0f, 0.0f);
			glVertex3f(-100.0f, static_cast<float>(x), 0.0f);
			glVertex3f(100.0f, static_cast<float>(x), 0.0f);
		}
		glEnd();
	} else {
		glBegin(GL_TRIANGLE_STRIP);
		glNormal3f(0.0f, 0.0f, 1.0f);
	
		glVertex3f(-100.0f, -100.0f, 0.0f);
		glVertex3f(100.0f, -100.0f, 0.0f);
		glVertex3f(-100.0f, 100.0f, 0.0f);
		glVertex3f(100.0f, 100.0f, 0.0f);

		glEnd();
	}
}

void debug_render::draw_body(const lotus::collision::shapes::sphere &sphere, const body_visual*, bool wireframe) {
	glPushMatrix();
	glScaled(2.0 * sphere.radius, 2.0 * sphere.radius, 2.0 * sphere.radius);
	glTranslated(sphere.offset[0], sphere.offset[1], sphere.offset[2]);
	if (wireframe) {
		draw_sphere_wireframe();
	} else {
		draw_sphere();
	}
	glPopMatrix();
}

void debug_render::draw_body(const lotus::collision::shapes::polyhedron &poly, const body_visual *visual, bool wireframe) {
	if (visual) {
		for (auto tri : visual->triangles) {
			auto p1 = poly.vertices[tri[0]];
			auto p2 = poly.vertices[tri[1]];
			auto p3 = poly.vertices[tri[2]];
			auto n = lotus::vec::cross(p2 - p1, p3 - p1);
			glNormal3d(n[0], n[1], n[2]);
			glBegin(wireframe ? GL_LINE_LOOP : GL_TRIANGLES);
			glVertex3d(p1[0], p1[1], p1[2]);
			glVertex3d(p2[0], p2[1], p2[2]);
			glVertex3d(p3[0], p3[1], p3[2]);
			glEnd();
		}
	} else {
		for (std::size_t i = 0; i < poly.vertices.size(); ++i) {
			auto p1 = poly.vertices[i];
			for (std::size_t j = i + 1; j < poly.vertices.size(); ++j) {
				auto p2 = poly.vertices[j];
				for (std::size_t k = j + 1; k < poly.vertices.size(); ++k) {
					auto p3 = poly.vertices[k];
					auto n = lotus::vec::cross(p2 - p1, p3 - p1);
					glNormal3d(n[0], n[1], n[2]);
					glBegin(wireframe ? GL_LINE_LOOP : GL_TRIANGLES);
					glVertex3d(p1[0], p1[1], p1[2]);
					glVertex3d(p2[0], p2[1], p2[2]);
					glVertex3d(p3[0], p3[1], p3[2]);
					glEnd();
				}
			}
		}
	}
}
