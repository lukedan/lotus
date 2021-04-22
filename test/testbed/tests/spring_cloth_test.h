#pragma once

#include <pbd/engine.h>

#include "test.h"
#include "../utils.h"

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
		double segment_length = _cloth_size / static_cast<double>(_side_segments - 1);

		auto &surface = _render.surfaces.emplace_back();
		surface.color = debug_render::colorf(1.0f, 0.4f, 0.2f, 0.5f);
		std::vector<std::vector<std::size_t>> pid(
			static_cast<std::size_t>(_side_segments),
			std::vector<std::size_t>(static_cast<std::size_t>(_side_segments)));
		for (int y = 0; y < _side_segments; ++y) {
			for (int x = 0; x < _side_segments; ++x) {
				auto prop = pbd::particle_properties::from_mass(node_mass);
				if (x == 0 && (y == 0 || y == _side_segments - 1)) {
					prop = pbd::particle_properties::kinematic();
				}
				auto state = pbd::particle_state::stationary_at(
					{ x * segment_length, y * segment_length - 0.5 * _cloth_size, _cloth_size }
				);
				pid[x][y] = _engine.particles.size();
				_engine.particles.emplace_back(pbd::particle::create(prop, state));
			}
		}
		for (int y = 0; y < _side_segments; ++y) {
			for (int x = 0; x < _side_segments; ++x) {
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

					surface.triangles.push_back({ pid[x - 1][y - 1], pid[x - 1][y], pid[x][y - 1] });
					surface.triangles.push_back({ pid[x][y - 1], pid[x - 1][y], pid[x][y] });
				}
			}
		}

		auto &sphere_shape = _engine.shapes.emplace_back(pbd::shape::create(pbd::shapes::sphere::from_radius(0.25)));

		auto material = pbd::material_properties::create(0.5, 0.45, 0.2);

		_engine.bodies.emplace_front(pbd::body::create(
			sphere_shape, material,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(pbd::zero, pbd::uquatd::identity())
		));
		_sphere = _engine.bodies.begin();
	}

	void timestep(double dt, std::size_t iterations) override {
		_world_time += dt;
		_sphere->state.position = {
			_sphere_travel * std::cos((2.0 * pbd::pi / _sphere_period) * _world_time),
			_sphere_yz[0],
			_sphere_yz[1]
		};
		_engine.timestep(dt, iterations);
	}

	void render(const draw_options &options) override {
		_render.draw(options);
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
		ImGui::Separator();

		ImGui::SliderFloat("Sphere Travel Distance", &_sphere_travel, 0.0f, 3.0f);
		ImGui::SliderFloat("Sphere Period", &_sphere_period, 0.1f, 10.0f);
		ImGui::SliderFloat2("Sphere Position", _sphere_yz, -10.0, 10.0);
		ImGui::Separator();

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

	std::list<pbd::body>::iterator _sphere;
	float _sphere_travel = 1.5f;
	float _sphere_period = 3.0f;
	float _sphere_yz[2]{ 0.0f, 0.5f };


	void _add_spring(std::size_t i1, std::size_t i2, double y) {
		auto &spring = _engine.particle_spring_constraints.emplace_back(pbd::uninitialized);
		spring.particle1 = i1;
		spring.particle2 = i2;
		spring.properties.length =
			(_engine.particles[i1].state.position - _engine.particles[i2].state.position).norm();
		spring.properties.inverse_stiffness = 1.0 / (spring.properties.length * y);
	}
};
