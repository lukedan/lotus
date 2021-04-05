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
		_render.engine = &_engine;

		_engine.gravity = { 0.0, 0.0, -10.0 };

		auto &surface = _render.surfaces.emplace_back();
		surface.color = debug_render::colorf(1.0f, 0.4f, 0.2f, 0.5f);
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

					surface.triangles.emplace_back(pid[x - 1][y - 1], pid[x - 1][y], pid[x][y - 1]);
					surface.triangles.emplace_back(pid[x][y - 1], pid[x - 1][y], pid[x][y]);
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
		_render.draw();
	}

	inline static std::string_view get_name() {
		return "Spring Cloth";
	}
protected:
	pbd::engine _engine;
	debug_render _render;
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
