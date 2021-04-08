#pragma once

#include <pbd/engine.h>

#include "../utils.h"
#include "test.h"

class spring_cloth_test : public test {
public:
	spring_cloth_test() {
		soft_reset();
	}

	void soft_reset() override {
		_engine = pbd::engine();
		_render = debug_render();
		_world_time = 0.0;

		_render.engine = &_engine;

		_engine.gravity = { 0.0, 0.0, -10.0 };

		double cloth_mass = _cloth_density * _cloth_size * _cloth_size * 0.001; // assume 1mm thick
		double node_mass = cloth_mass / (_side_segments * _side_segments);
		double segment_length = _cloth_size / (_side_segments - 1);

		auto &surface = _render.surfaces.emplace_back();
		surface.color = debug_render::colorf(1.0f, 0.4f, 0.2f, 0.5f);
		std::vector<std::vector<std::size_t>> pid(_side_segments, std::vector<std::size_t>(_side_segments));
		for (std::size_t y = 0; y < _side_segments; ++y) {
			for (std::size_t x = 0; x < _side_segments; ++x) {
				auto prop = pbd::particle_properties::from_mass(node_mass);
				if (x == 0 && (y == 0 || y == _side_segments - 1)) {
					prop = pbd::particle_properties::kinematic();
				}
				pbd::particle_state state(
					{ x * segment_length, y * segment_length - 0.5 * _cloth_size, _cloth_size }, pbd::zero
				);
				pid[x][y] = _engine.particles.size();
				_engine.particles.emplace_back(prop, state);
			}
		}
		for (std::size_t y = 0; y < _side_segments; ++y) {
			for (std::size_t x = 0; x < _side_segments; ++x) {
				if (x > 0) {
					_add_spring(pid[x - 1][y], pid[x][y], _youngs_modulus_short);
					if (x > 1) {
						_add_spring(pid[x - 2][y], pid[x][y], _youngs_modulus_long);
					}

				}

				if (y > 0) {
					_add_spring(pid[x][y - 1], pid[x][y], _youngs_modulus_short);
					if (y > 1) {
						_add_spring(pid[x][y - 2], pid[x][y], _youngs_modulus_long);
					}
				}

				if (x > 0 && y > 0) {
					_add_spring(pid[x - 1][y - 1], pid[x][y], _youngs_modulus_diag);
					_add_spring(pid[x - 1][y], pid[x][y - 1], _youngs_modulus_diag);

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

	void gui() override {
		ImGui::SliderInt("Cloth Partitions", &_side_segments, 2, 100);
		ImGui::SliderFloat("Cloth Size", &_cloth_size, 0.0f, 3.0f);
		ImGui::SliderFloat("Cloth Density", &_cloth_density, 0.0f, 20000.0f);
		ImGui::SliderFloat(
			"Young's Modulus - Short", &_youngs_modulus_short, 0.0f, 1000000000.0f,
			"%f", ImGuiSliderFlags_Logarithmic
		);
		ImGui::SliderFloat(
			"Young's Modulus - Diagonal", &_youngs_modulus_diag, 0.0f, 1000000000.0f,
			"%f", ImGuiSliderFlags_Logarithmic
		);
		ImGui::SliderFloat(
			"Young's Modulus - Long", &_youngs_modulus_long, 0.0f, 1000000000.0f,
			"%f", ImGuiSliderFlags_Logarithmic
		);

		test::gui();
	}

	inline static std::string_view get_name() {
		return "Spring Cloth";
	}
protected:
	pbd::engine _engine;
	debug_render _render;
	double _world_time = 0.0;

	int _side_segments = 30;
	float _cloth_size = 1.0f;
	float _cloth_density = 1200.0f;

	float _youngs_modulus_short = 50000.0f;
	float _youngs_modulus_diag = 50000.0f;
	float _youngs_modulus_long = 50000.0f;


	void _add_spring(std::size_t i1, std::size_t i2, double y) {
		auto &spring = _engine.spring_constraints.emplace_back(pbd::uninitialized);
		spring.object1 = i1;
		spring.object2 = i2;
		spring.properties.length =
			(_engine.particles[i1].state.position - _engine.particles[i2].state.position).norm();
		spring.properties.inverse_stiffness = 1.0 / (spring.properties.length * y);
	}
};
