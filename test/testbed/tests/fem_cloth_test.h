#pragma once

#include <pbd/engine.h>

#include "../utils.h"
#include "test.h"

class fem_cloth_test : public test {
public:
	constexpr static std::size_t side_segments = 10;
	constexpr static double cloth_size = 1.0;
	constexpr static double cloth_mass = 10.0;
	constexpr static double node_mass = cloth_mass / (side_segments * side_segments);
	constexpr static double segment_length = cloth_size / (side_segments - 1);

	constexpr static double youngs_modulus = 10000000;
	constexpr static double poisson_ratio = 0.3;
	constexpr static double thickness = 0.2;


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

				_add_bend(pid[x][y - 1], pid[x - 1][y], pid[x - 1][y - 1], pid[x][y]);
				if (x > 1) {
					_add_bend(pid[x - 1][y - 1], pid[x - 1][y], pid[x - 2][y], pid[x][y - 1]);
				}
				if (y > 1) {
					_add_bend(pid[x - 1][y - 1], pid[x][y - 1], pid[x][y - 2], pid[x - 1][y]);
				}

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
		_engine.bodies[0].state.position = { 1.5 * std::cos(1.3 * _world_time), 0.0, 0.5 };
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
			thickness
		);
		face.properties = pbd::constraints::face::constraint_properties::from_material_properties(
			youngs_modulus, poisson_ratio
		);
	}

	void _add_bend(std::size_t e1, std::size_t e2, std::size_t x3, std::size_t x4) {
		auto &bend = _engine.bend_constraints.emplace_back(pbd::uninitialized);
		bend.particle_edge1 = e1;
		bend.particle_edge2 = e2;
		bend.particle3 = x3;
		bend.particle4 = x4;
		bend.state = pbd::constraints::bend::constraint_state::from_rest_pose(
			_engine.particles[e1].state.position,
			_engine.particles[e2].state.position,
			_engine.particles[x3].state.position,
			_engine.particles[x4].state.position,
			thickness
		);
		bend.properties = pbd::constraints::bend::constraint_properties::from_material_properties(
			youngs_modulus, poisson_ratio
		);
	}
};
