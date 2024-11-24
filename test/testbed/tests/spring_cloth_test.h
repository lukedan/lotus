#pragma once

#include <lotus/physics/engine.h>

#include "test.h"
#include "../utils.h"

class spring_cloth_test : public test {
public:
	explicit spring_cloth_test(const test_context &tctx) : test(tctx) {
		soft_reset();
	}

	void soft_reset() override {
		_engine = lotus::physics::engine();
		_engine.gravity = { 0.0, -10.0, 0.0 };

		_render = debug_render();
		_render.ctx = &_get_test_context();

		_world_time = 0.0;


		double cloth_mass = _cloth_density * _cloth_size * _cloth_size * 0.001; // assume 1mm thick
		double node_mass = cloth_mass / (_side_segments * _side_segments);
		double segment_length = _cloth_size / static_cast<double>(_side_segments - 1);

		auto &surface = _render.surfaces.emplace_back();
		surface.color = lotus::linear_rgba_f(1.0f, 0.4f, 0.2f, 0.5f);
		std::vector<std::vector<std::uint32_t>> pid(
			static_cast<std::size_t>(_side_segments),
			std::vector<std::uint32_t>(static_cast<std::size_t>(_side_segments)));
		for (int y = 0; y < _side_segments; ++y) {
			for (int x = 0; x < _side_segments; ++x) {
				auto prop = lotus::physics::particle_properties::from_mass(node_mass);
				if (x == 0 && (y == 0 || y == _side_segments - 1)) {
					prop = lotus::physics::particle_properties::kinematic();
				}
				auto state = lotus::physics::particle_state::stationary_at(
					{ x * segment_length, _cloth_size, y * segment_length - 0.5 * _cloth_size }
				);
				pid[x][y] = static_cast<std::uint32_t>(_engine.particles.size());
				_engine.particles.emplace_back(lotus::physics::particle::create(prop, state));
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

					surface.triangles.append_range(std::vector{ pid[x - 1][y - 1], pid[x - 1][y], pid[x][y - 1] });
					surface.triangles.append_range(std::vector{ pid[x][y - 1], pid[x - 1][y], pid[x][y] });
				}
			}
		}

		auto &sphere_shape = _engine.shapes.emplace_back(
			lotus::collision::shape::create(lotus::collision::shapes::sphere::from_radius(0.25))
		);

		auto material = lotus::physics::material_properties(0.5, 0.45, 0.2);

		_engine.bodies.emplace_front(lotus::physics::body::create(
			sphere_shape, material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(lotus::zero, uquats::identity())
		));
		_sphere = _engine.bodies.begin();
	}

	void timestep(double dt, std::size_t iterations) override {
		_world_time += dt;
		_sphere->state.position = {
			_sphere_travel * std::cos((2.0 * lotus::pi / _sphere_period) * _world_time),
			_sphere_yz[0],
			_sphere_yz[1]
		};
		_engine.timestep(dt, iterations);
	}

	void render(
		lotus::renderer::context &ctx, lotus::renderer::context::queue &q,
		lotus::renderer::constant_uploader &uploader,
		lotus::renderer::image2d_color color, lotus::renderer::image2d_depth_stencil depth, lotus::cvec2u32 size
	) override {
		_render.draw_system(_engine);
		_render.flush(ctx, q, uploader, color, depth, size);
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
	lotus::physics::engine _engine;
	debug_render _render;
	double _world_time = 0.0;

	int _side_segments = 30;
	float _cloth_size = 1.0f;
	float _cloth_density = 1200.0f;

	float _youngs_modulus_short = 50000.0f;
	float _youngs_modulus_diag = 50000.0f;
	float _youngs_modulus_long = 50000.0f;

	std::list<lotus::physics::body>::iterator _sphere;
	float _sphere_travel = 1.5f;
	float _sphere_period = 3.0f;
	float _sphere_yz[2]{ 0.5f, 0.0f };


	void _add_spring(std::size_t i1, std::size_t i2, double y) {
		auto &spring = _engine.particle_spring_constraints.emplace_back(lotus::uninitialized);
		spring.particle1 = i1;
		spring.particle2 = i2;
		spring.properties.length =
			(_engine.particles[i1].state.position - _engine.particles[i2].state.position).norm();
		spring.properties.inverse_stiffness = 1.0 / (spring.properties.length * y);
	}
};
