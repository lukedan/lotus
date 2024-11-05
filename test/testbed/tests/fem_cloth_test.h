#pragma once

#include <lotus/physics/engine.h>

#include "test.h"
#include "../utils.h"

class fem_cloth_test : public test {
public:
	explicit fem_cloth_test(const test_context &tctx) : test(tctx) {
		soft_reset();
	}

	void soft_reset() override {
		_engine = lotus::physics::engine();
		_engine.gravity = { 0.0, -10.0, 0.0 };
		_engine.face_constraint_projection_type =
			static_cast<lotus::physics::constraints::face::projection_type>(_face_projection);

		_render = debug_render();
		_render.ctx = &_get_test_context();

		_world_time = 0.0;

		double cloth_mass = _cloth_density * _cloth_size * _cloth_size * _thickness;
		double node_mass = cloth_mass / (_side_segments * _side_segments);
		double segment_length = _cloth_size / static_cast<double>(_side_segments - 1);

		auto &surface = _render.surfaces.emplace_back();
		surface.color = lotus::linear_rgba_f(1.0f, 0.4f, 0.2f, 0.5f);
		std::vector<std::vector<std::size_t>> pid(
			static_cast<std::size_t>(_side_segments),
			std::vector<std::size_t>(static_cast<std::size_t>(_side_segments))
		);
		for (int y = 0; y < _side_segments; ++y) {
			for (int x = 0; x < _side_segments; ++x) {
				auto prop = lotus::physics::particle_properties::from_mass(node_mass);
				if (x == 0 && (y == 0 || y == _side_segments - 1)) {
					prop = lotus::physics::particle_properties::kinematic();
				}
				auto state = lotus::physics::particle_state::stationary_at(
					{ x * segment_length, _cloth_size, y * segment_length - 0.5 * _cloth_size }
				);
				pid[x][y] = _engine.particles.size();
				_engine.particles.emplace_back(lotus::physics::particle::create(prop, state));
			}
		}
		for (int y = 1; y < _side_segments; ++y) {
			for (int x = 1; x < _side_segments; ++x) {
				_add_face(pid[x - 1][y - 1], pid[x - 1][y], pid[x][y - 1]);
				_add_face(pid[x - 1][y], pid[x][y], pid[x][y - 1]);

				if (_bend_constraints) {
					_add_bend(pid[x][y - 1], pid[x - 1][y], pid[x - 1][y - 1], pid[x][y]);
					if (x > 1) {
						_add_bend(pid[x - 1][y - 1], pid[x - 1][y], pid[x - 2][y], pid[x][y - 1]);
					}
					if (y > 1) {
						_add_bend(pid[x - 1][y - 1], pid[x][y - 1], pid[x][y - 2], pid[x - 1][y]);
					}
				}

				surface.triangles.append_range(std::vector{ pid[x - 1][y - 1], pid[x - 1][y], pid[x][y - 1] });
				surface.triangles.append_range(std::vector{ pid[x][y - 1], pid[x - 1][y], pid[x][y] });
			}
		}

		auto &sphere_shape = _engine.shapes.emplace_back(lotus::collision::shape::create(lotus::collision::shapes::sphere::from_radius(0.25)));
		auto &plane_shape = _engine.shapes.emplace_back(lotus::collision::shape::create(lotus::collision::shapes::plane()));

		auto material = lotus::physics::material_properties::create(0.5, 0.45, 0.2);

		_engine.bodies.emplace_front(lotus::physics::body::create(
			sphere_shape, material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(lotus::zero, lotus::uquatd::identity())
		));
		_sphere = _engine.bodies.begin();

		_engine.bodies.emplace_front(lotus::physics::body::create(
			plane_shape, material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(lotus::zero, lotus::quat::from_axis_angle(lotus::cvec3d(1.0f, 0.0f, 0.0f), -0.5f * lotus::pi))
		));
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
		if (ImGui::Combo("Face Constraint Projection", &_face_projection, "Exact\0Gauss-Seidel\0\0")) {
			_engine.face_constraint_projection_type =
				static_cast<lotus::physics::constraints::face::projection_type>(_face_projection);
		}

		ImGui::SliderInt("Cloth Partitions", &_side_segments, 2, 100);
		ImGui::SliderFloat("Cloth Size", &_cloth_size, 0.0f, 3.0f);
		ImGui::SliderFloat("Cloth Density", &_cloth_density, 0.0f, 20000.0f);
		ImGui::SliderFloat(
			"Young's Modulus", &_youngs_modulus, 0.0f, 1000000000.0f, "%.0f", ImGuiSliderFlags_Logarithmic
		);
		ImGui::SliderFloat("Poisson's Ratio", &_poisson_ratio, 0.0f, 0.5f);
		ImGui::SliderFloat("Thickness", &_thickness, 0.0f, 0.1f);
		ImGui::Checkbox("Bending Constraints", &_bend_constraints);
		ImGui::Separator();

		ImGui::SliderFloat("Sphere Travel Distance", &_sphere_travel, 0.0f, 3.0f);
		ImGui::SliderFloat("Sphere Period", &_sphere_period, 0.1f, 10.0f);
		ImGui::SliderFloat2("Sphere Position", _sphere_yz, -10.0, 10.0);
		ImGui::Separator();

		test::gui();
	}

	inline static std::string_view get_name() {
		return "FEM Cloth";
	}
protected:
	lotus::physics::engine _engine;
	debug_render _render;
	double _world_time = 0.0;

	int _face_projection = static_cast<int>(lotus::physics::constraints::face::projection_type::gauss_seidel);

	int _side_segments = 10;
	float _cloth_size = 1.0f;
	float _cloth_density = 1200.0f;
	float _youngs_modulus = 10000000.0f;
	float _poisson_ratio = 0.3f;
	float _thickness = 0.02f;
	bool _bend_constraints = true;

	std::list<lotus::physics::body>::iterator _sphere;
	float _sphere_travel = 1.5f;
	float _sphere_period = 3.0f;
	float _sphere_yz[2]{ 0.5f, 0.0f };


	void _add_face(std::size_t i1, std::size_t i2, std::size_t i3) {
		auto &face = _engine.face_constraints.emplace_back(lotus::uninitialized);
		face.particle1 = i1;
		face.particle2 = i2;
		face.particle3 = i3;
		face.state = lotus::physics::constraints::face::constraint_state::from_rest_pose(
			_engine.particles[i1].state.position,
			_engine.particles[i2].state.position,
			_engine.particles[i3].state.position,
			_thickness
		);
		face.properties = lotus::physics::constraints::face::constraint_properties::from_material_properties(
			_youngs_modulus, _poisson_ratio
		);
	}

	void _add_bend(std::size_t e1, std::size_t e2, std::size_t x3, std::size_t x4) {
		auto &bend = _engine.bend_constraints.emplace_back(lotus::uninitialized);
		bend.particle_edge1 = e1;
		bend.particle_edge2 = e2;
		bend.particle3 = x3;
		bend.particle4 = x4;
		bend.state = lotus::physics::constraints::bend::constraint_state::from_rest_pose(
			_engine.particles[e1].state.position,
			_engine.particles[e2].state.position,
			_engine.particles[x3].state.position,
			_engine.particles[x4].state.position
		);
		bend.properties = lotus::physics::constraints::bend::constraint_properties::from_material_properties(
			_youngs_modulus, _poisson_ratio, _thickness
		);
	}
};
