#pragma once

#include <pbd/engine.h>

#include "../utils.h"
#include "test.h"

class fem_cloth_test : public test {
public:
	constexpr static std::size_t side_segments = 30;
	constexpr static double cloth_size = 1.0;
	constexpr static double cloth_mass = 10.0;
	constexpr static double node_mass = cloth_mass / (side_segments * side_segments);
	constexpr static double segment_length = cloth_size / (side_segments - 1);

	constexpr static double youngs_modulus_short = 50000;
	constexpr static double youngs_modulus_diag = 50000;
	constexpr static double youngs_modulus_long = 500;

	fem_cloth_test() {
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
		for (std::size_t y = 1; y < side_segments; ++y) {
			for (std::size_t x = 1; x < side_segments; ++x) {
				_add_face(pid[x - 1][y - 1], pid[x - 1][y], pid[x][y - 1]);
				_add_face(pid[x - 1][y], pid[x][y], pid[x][y - 1]);

				surface.triangles.emplace_back(pid[x - 1][y - 1], pid[x - 1][y], pid[x][y - 1]);
				surface.triangles.emplace_back(pid[x][y - 1], pid[x - 1][y], pid[x][y]);
			}
		}

		auto &sphere = _engine.bodies.emplace_back(pbd::uninitialized);
		sphere.properties = pbd::body_properties::kinematic();
		sphere.body_shape.value = pbd::shapes::sphere::from_radius(0.25);
		sphere.state.position = pbd::zero;

		auto &plane = _engine.bodies.emplace_back(pbd::uninitialized);
		plane.properties = pbd::body_properties::kinematic();
		plane.body_shape.value.emplace<pbd::shapes::plane>();
		plane.state.position = pbd::zero;
		plane.state.rotation = pbd::quatd::identity();
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
		return "FEM Cloth";
	}
protected:
	pbd::engine _engine;
	debug_render _render;
	double _time = 0.0;
	double _world_time = 0.0;

	void _add_face(std::size_t i1, std::size_t i2, std::size_t i3) {
		auto &face = _engine.face_constraints.emplace_back(pbd::uninitialized);
		face.particle1 = i1;
		face.particle2 = i2;
		face.particle3 = i3;
		face.state = pbd::constraints::face::constraint_state::from_rest_pose(
			_engine.particles[i1].state.position,
			_engine.particles[i2].state.position,
			_engine.particles[i3].state.position,
			0.1
		);
		face.properties = pbd::constraints::face::constraint_properties::from_material_properties(
			100000, 0.3
		);
	}
};
