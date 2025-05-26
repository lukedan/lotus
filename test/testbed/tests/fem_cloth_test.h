#pragma once

#include <lotus/physics/xpbd/solver.h>

#include "test.h"
#include "../utils.h"

class fem_cloth_test : public test {
public:
	explicit fem_cloth_test(const test_context &tctx) : test(tctx) {
		soft_reset();
	}

	void soft_reset() override {
		_bodies.clear();
		_world = lotus::physics::world();
		_world.gravity = { 0.0f, -10.0f, 0.0f };
		_engine = lotus::physics::xpbd::solver();
		_engine.physics_world = &_world;
		_engine.face_constraint_projection_type =
			static_cast<lotus::physics::xpbd::constraints::face::projection_type>(_face_projection);

		_render = debug_render();
		_render.ctx = &_get_test_context();

		_world_time = 0.0f;

		const auto side_segs = static_cast<usize>(_side_segments);
		scalar cloth_mass = _cloth_density * _cloth_size * _cloth_size * _thickness;
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
		for (usize y = 1; y < side_segs; ++y) {
			for (usize x = 1; x < side_segs; ++x) {
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

		_sphere_shape = lotus::collision::shape::create(lotus::collision::shapes::sphere::from_radius(0.25));
		_plane_shape = lotus::collision::shape::create(lotus::collision::shapes::plane());

		auto material = lotus::physics::material_properties(0.5f, 0.45f, 0.2f);

		_sphere = &_bodies.emplace_front(lotus::physics::body::create(
			_sphere_shape, material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(lotus::zero, lotus::physics::uquats::identity())
		));
		_world.add_body(*_sphere);

		_world.add_body(_bodies.emplace_front(lotus::physics::body::create(
			_plane_shape, material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(lotus::zero, lotus::quat::from_axis_angle(lotus::physics::vec3(1.0f, 0.0f, 0.0f), -0.5f * lotus::physics::pi))
		)));
	}

	void timestep(scalar dt, u32 iterations) override {
		_world_time += dt;
		_sphere->state.position.position = {
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
		if (ImGui::Combo("Face Constraint Projection", &_face_projection, "Exact\0Gauss-Seidel\0\0")) {
			_engine.face_constraint_projection_type =
				static_cast<lotus::physics::xpbd::constraints::face::projection_type>(_face_projection);
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
	std::deque<lotus::physics::body> _bodies;
	lotus::physics::world _world;
	lotus::physics::xpbd::solver _engine;
	debug_render _render;
	scalar _world_time = 0.0f;

	int _face_projection = static_cast<int>(lotus::physics::xpbd::constraints::face::projection_type::gauss_seidel);

	int _side_segments = 10;
	float _cloth_size = 1.0f;
	float _cloth_density = 1200.0f;
	float _youngs_modulus = 10000000.0f;
	float _poisson_ratio = 0.3f;
	float _thickness = 0.02f;
	bool _bend_constraints = true;

	lotus::physics::body *_sphere = nullptr;
	float _sphere_travel = 1.5f;
	float _sphere_period = 3.0f;
	float _sphere_yz[2]{ 0.5f, 0.0f };

	lotus::collision::shape _sphere_shape;
	lotus::collision::shape _plane_shape;


	void _add_face(usize i1, usize i2, usize i3) {
		auto &face = _engine.face_constraints.emplace_back(lotus::uninitialized);
		face.particle1 = i1;
		face.particle2 = i2;
		face.particle3 = i3;
		face.state = lotus::physics::xpbd::constraints::face::constraint_state::from_rest_pose(
			_engine.particles[i1].state.position,
			_engine.particles[i2].state.position,
			_engine.particles[i3].state.position,
			_thickness
		);
		face.properties = lotus::physics::xpbd::constraints::face::constraint_properties::from_material_properties(
			_youngs_modulus, _poisson_ratio
		);
	}

	void _add_bend(usize e1, usize e2, usize x3, usize x4) {
		auto &bend = _engine.bend_constraints.emplace_back(lotus::uninitialized);
		bend.particle_edge1 = e1;
		bend.particle_edge2 = e2;
		bend.particle3 = x3;
		bend.particle4 = x4;
		bend.state = lotus::physics::xpbd::constraints::bend::constraint_state::from_rest_pose(
			_engine.particles[e1].state.position,
			_engine.particles[e2].state.position,
			_engine.particles[x3].state.position,
			_engine.particles[x4].state.position
		);
		bend.properties = lotus::physics::xpbd::constraints::bend::constraint_properties::from_material_properties(
			_youngs_modulus, _poisson_ratio, _thickness
		);
	}
};
