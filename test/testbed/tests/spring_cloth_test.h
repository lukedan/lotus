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
		_engine.gravity = { 0.0f, -10.0f, 0.0f };

		_render = debug_render();
		_render.ctx = &_get_test_context();

		_world_time = 0.0f;


		auto side_segs = static_cast<usize>(_side_segments);

		scalar cloth_mass = _cloth_density * _cloth_size * _cloth_size * 0.001f; // assume 1mm thick
		scalar node_mass = cloth_mass / (side_segs * side_segs);
		scalar segment_length = _cloth_size / static_cast<scalar>(side_segs - 1);

		auto &surface = _render.surfaces.emplace_back();
		surface.color = lotus::linear_rgba_f(1.0f, 0.4f, 0.2f, 0.5f);
		std::vector<std::vector<u32>> pid(side_segs, std::vector<u32>(side_segs));
		for (usize y = 0; y < side_segs; ++y) {
			for (usize x = 0; x < side_segs; ++x) {
				auto prop = lotus::physics::particle_properties::from_mass(node_mass);
				if (x == 0 && (y == 0 || y == side_segs - 1)) {
					prop = lotus::physics::particle_properties::kinematic();
				}
				auto state = lotus::physics::particle_state::stationary_at(
					{ x * segment_length, _cloth_size, y * segment_length - 0.5f * _cloth_size }
				);
				pid[x][y] = static_cast<u32>(_engine.particles.size());
				_engine.particles.emplace_back(lotus::physics::particle::create(prop, state));
			}
		}
		for (usize y = 0; y < side_segs; ++y) {
			for (usize x = 0; x < side_segs; ++x) {
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
			lotus::collision::shape::create(lotus::collision::shapes::sphere::from_radius(0.25f))
		);

		auto material = lotus::physics::material_properties(0.5f, 0.45f, 0.2f);

		_engine.bodies.emplace_front(lotus::physics::body::create(
			sphere_shape, material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(lotus::zero, uquats::identity())
		));
		_sphere = _engine.bodies.begin();
	}

	void timestep(scalar dt, u32 iterations) override {
		_world_time += dt;
		_sphere->state.position = {
			_sphere_travel * std::cos((2.0f * lotus::physics::pi / _sphere_period) * _world_time),
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
	scalar _world_time = 0.0f;

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


	void _add_spring(usize i1, usize i2, scalar y) {
		auto &spring = _engine.particle_spring_constraints.emplace_back(lotus::uninitialized);
		spring.particle1 = i1;
		spring.particle2 = i2;
		spring.properties.length =
			(_engine.particles[i1].state.position - _engine.particles[i2].state.position).norm();
		spring.properties.inverse_stiffness = 1.0f / (spring.properties.length * y);
	}
};
