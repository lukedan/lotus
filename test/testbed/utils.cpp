#include "utils.h"

#include <vector>

#include <GLFW/glfw3.h>

#include <pbd/math/constants.h>

void set_matrix(pbd::mat44d mat) {
	GLdouble values[16]{
		mat(0, 0), mat(1, 0), mat(2, 0), mat(3, 0),
		mat(0, 1), mat(1, 1), mat(2, 1), mat(3, 1),
		mat(0, 2), mat(1, 2), mat(2, 2), mat(3, 2),
		mat(0, 3), mat(1, 3), mat(2, 3), mat(3, 3)
	};
	glLoadMatrixd(values);
}

void draw_sphere() {
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
