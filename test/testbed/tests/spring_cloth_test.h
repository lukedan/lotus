#pragma once

#include <pbd/engine.h>

#include "../utils.h"
#include "test.h"

class spring_cloth_test : public test {
public:
	constexpr static std::size_t side_segments = 30;
	constexpr static double cloth_size = 1.0;
	constexpr static double cloth_mass = 10.0;
	constexpr static double node_mass = cloth_mass / (side_segments * side_segments);
	constexpr static double segment_length = cloth_size / (side_segments - 1);

	constexpr static double youngs_modulus_short = 50000;
	constexpr static double youngs_modulus_diag = 50000;
	constexpr static double youngs_modulus_long = 500;

	spring_cloth_test() {
		_engine.gravity = { 0.0, 0.0, -10.0 };

		std::size_t pid[side_segments][side_segments];
		for (std::size_t y = 0; y < side_segments; ++y) {
			for (std::size_t x = 0; x < side_segments; ++x) {
				auto prop = pbd::particle_properties::from_mass(node_mass);
				if (x == 0 && (y == 0 || y == side_segments - 1)) {
					prop = pbd::particle_properties::kinematic();
				}
				pbd::particle_state state(
					{ x * segment_length, y * segment_length - 0.5 * cloth_size, cloth_size }, pbd::zero
				);
				pid[x][y] = _engine.particles.size();
				_engine.particles.emplace_back(prop, state);
			}
		}
		for (std::size_t y = 0; y < side_segments; ++y) {
			for (std::size_t x = 0; x < side_segments; ++x) {
				if (x > 0) {
					_add_spring(pid[x - 1][y], pid[x][y], youngs_modulus_short);
					if (x > 1) {
						_add_spring(pid[x - 2][y], pid[x][y], youngs_modulus_long);
					}

				}

				if (y > 0) {
					_add_spring(pid[x][y - 1], pid[x][y], youngs_modulus_short);
					if (y > 1) {
						_add_spring(pid[x][y - 2], pid[x][y], youngs_modulus_long);
					}
				}

				if (x > 0 && y > 0) {
					_add_spring(pid[x - 1][y - 1], pid[x][y], youngs_modulus_diag);
					_add_spring(pid[x - 1][y], pid[x][y - 1], youngs_modulus_diag);

					_triangles.emplace_back(pid[x - 1][y - 1]);
					_triangles.emplace_back(pid[x - 1][y]);
					_triangles.emplace_back(pid[x][y - 1]);

					_triangles.emplace_back(pid[x][y - 1]);
					_triangles.emplace_back(pid[x - 1][y]);
					_triangles.emplace_back(pid[x][y]);
				}
			}
		}

		auto &sphere = _engine.bodies.emplace_back(pbd::uninitialized);
		sphere.properties = pbd::body_properties::kinematic();
		sphere.body_shape.value = pbd::shapes::sphere::from_radius(0.25);
		sphere.state.position = pbd::zero;
	}

	void timestep(double dt, std::size_t iterations) override {
		_world_time += dt;
		_engine.bodies[0].state.position = { std::cos(1.3 * _world_time), 0.0, 0.5 };
		_engine.timestep(dt, iterations);
	}

	void render() override {
		// compute normals
		std::vector<pbd::cvec3d> normals(_engine.particles.size(), pbd::zero);
		for (std::size_t i = 0; i + 2 < _triangles.size(); i += 3) {
			auto p1 = _engine.particles[_triangles[i]].state.position;
			auto p2 = _engine.particles[_triangles[i + 1]].state.position;
			auto p3 = _engine.particles[_triangles[i + 2]].state.position;
			auto diff = pbd::vec::cross(p2 - p1, p3 - p1);
			normals[_triangles[i]] += diff;
			normals[_triangles[i + 1]] += diff;
			normals[_triangles[i + 2]] += diff;
		}

		glColor3d(1.0, 0.4, 0.2);
		glLoadIdentity();
		glBegin(GL_TRIANGLES);
		for (std::size_t i = 0; i + 2 < _triangles.size(); i += 3) {
			auto n1 = normals[_triangles[i]];
			auto n2 = normals[_triangles[i + 1]];
			auto n3 = normals[_triangles[i + 2]];
			auto p1 = _engine.particles[_triangles[i]].state.position;
			auto p2 = _engine.particles[_triangles[i + 1]].state.position;
			auto p3 = _engine.particles[_triangles[i + 2]].state.position;
			glNormal3d(n1[0], n1[1], n1[2]);
			glVertex3d(p1[0], p1[1], p1[2]);
			glNormal3d(n2[0], n2[1], n2[2]);
			glVertex3d(p2[0], p2[1], p2[2]);
			glNormal3d(n3[0], n3[1], n3[2]);
			glVertex3d(p3[0], p3[1], p3[2]);
		}
		glEnd();

		glColor3d(0.8, 0.8, 0.8);
		auto p = _engine.bodies[0].state.position;
		set_matrix({
			{ 0.5, 0.0, 0.0, p[0] },
			{ 0.0, 0.5, 0.0, p[1] },
			{ 0.0, 0.0, 0.5, p[2] },
			{ 0.0, 0.0, 0.0, 1.0 }
		});
		draw_sphere();
	}

	inline static std::string_view get_name() {
		return "Spring Cloth";
	}
protected:
	pbd::engine _engine;
	std::vector<std::size_t> _triangles;
	double _time = 0.0;
	double _world_time = 0.0;

	void _add_spring(std::size_t i1, std::size_t i2, double y) {
		auto &spring = _engine.spring_constraints.emplace_back(pbd::uninitialized);
		spring.object1 = i1;
		spring.object2 = i2;
		spring.properties.length =
			(_engine.particles[i1].state.position - _engine.particles[i2].state.position).norm();
		spring.properties.inverse_stiffness = 1.0 / (spring.properties.length * y);
	}
};
