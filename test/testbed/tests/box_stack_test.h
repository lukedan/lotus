#pragma once

#include <lotus/physics/sequential_impulse/solver.h>
#include <lotus/physics/xpbd/solver.h>
#include <lotus/physics/world.h>

#include "test.h"

class box_stack_test : public test {
public:
	explicit box_stack_test(const test_context &tctx) : test(tctx) {
		soft_reset();
	}

	void timestep(scalar dt, u32 iters) override {
		_solver.timestep(dt, iters);
	}

	void render(
		lotus::renderer::context &ctx, lotus::renderer::context::queue &q,
		lotus::renderer::constant_uploader &uploader,
		lotus::renderer::image2d_color color, lotus::renderer::image2d_depth_stencil depth, lotus::cvec2u32 size
	) override {
		_render.draw_system(_solver);
		_render.flush(ctx, q, uploader, color, depth, size);
	}

	void soft_reset() override {
		_bodies.clear();
		_world = lotus::physics::world();
		_world.gravity = vec3(0.0f, -9.8f, 0.0f);
		_solver = decltype(_solver)();
		_solver.physics_world = &_world;

		_render = debug_render();
		_render.ctx = &_get_test_context();

		_plane_shape = lotus::collision::shape::create(lotus::collision::shapes::plane());

		lotus::physics::body_properties box_props = lotus::uninitialized;
		{
			std::vector<vec3> box_verts;
			vec3 half_size(_box_size[0], _box_size[1], _box_size[2]);
			half_size *= 0.5f;
			box_verts.emplace_back( half_size[0],  half_size[1],  half_size[2]);
			box_verts.emplace_back( half_size[0],  half_size[1], -half_size[2]);
			box_verts.emplace_back( half_size[0], -half_size[1],  half_size[2]);
			box_verts.emplace_back( half_size[0], -half_size[1], -half_size[2]);
			box_verts.emplace_back(-half_size[0],  half_size[1],  half_size[2]);
			box_verts.emplace_back(-half_size[0],  half_size[1], -half_size[2]);
			box_verts.emplace_back(-half_size[0], -half_size[1],  half_size[2]);
			box_verts.emplace_back(-half_size[0], -half_size[1], -half_size[2]);
			auto [box_poly, box_poly_props] = lotus::collision::shapes::convex_polyhedron::bake(box_verts);
			_box_shape = lotus::collision::shape::create(std::move(box_poly));
			box_props = box_poly_props.get_body_properties(1.0f);
		}

		{
			std::vector<vec3> bullet_verts;
			vec3 half_bullet_size(0.05f, 0.05f, 0.05f);
			bullet_verts.emplace_back(half_bullet_size[0], half_bullet_size[1], half_bullet_size[2]);
			bullet_verts.emplace_back(half_bullet_size[0], half_bullet_size[1], -half_bullet_size[2]);
			bullet_verts.emplace_back(half_bullet_size[0], -half_bullet_size[1], half_bullet_size[2]);
			bullet_verts.emplace_back(half_bullet_size[0], -half_bullet_size[1], -half_bullet_size[2]);
			bullet_verts.emplace_back(-half_bullet_size[0], half_bullet_size[1], half_bullet_size[2]);
			bullet_verts.emplace_back(-half_bullet_size[0], half_bullet_size[1], -half_bullet_size[2]);
			bullet_verts.emplace_back(-half_bullet_size[0], -half_bullet_size[1], half_bullet_size[2]);
			bullet_verts.emplace_back(-half_bullet_size[0], -half_bullet_size[1], -half_bullet_size[2]);
			auto [bullet_poly, bullet_poly_props] = lotus::collision::shapes::convex_polyhedron::bake(bullet_verts);
			_bullet_shape = lotus::collision::shape::create(std::move(bullet_poly));
			_bullet_properties = bullet_poly_props.get_body_properties(1000.0f);
		}

		auto material = lotus::physics::material_properties(
			_static_friction, _dynamic_friction, _restitution
		);

		_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
			_plane_shape, material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(
				lotus::zero, lotus::quat::from_normalized_axis_angle(vec3(1.0f, 0.0f, 0.0f), -0.5f * lotus::physics::pi)
			)
		)));
		if (_add_walls) {
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_plane_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					vec3(10.0f, 0.0f, 0.0f),
					lotus::quat::from_normalized_axis_angle(vec3(0.0f, 1.0f, 0.0f), -0.5f * lotus::physics::pi)
				)
			)));
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_plane_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					vec3(-10.0f, 0.0f, 0.0f),
					lotus::quat::from_normalized_axis_angle(vec3(0.0f, 1.0f, 0.0f), 0.5f * lotus::physics::pi)
				)
			)));
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_plane_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					vec3(0.0f, 0.0f, 10.0f),
					lotus::quat::from_normalized_axis_angle(vec3(0.0f, 1.0f, 0.0f), lotus::physics::pi)
				)
			)));
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_plane_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					vec3(0.0f, 0.0f, -10.0f), uquats::identity()
				)
			)));
		}

		scalar x = -(_box_size[0] + _gap[0]) * (_box_count[0] - 1) / 2.0f;
		scalar y = 0.5f * _box_size[1] + _gap[1];
		for (
			int yi = 0;
			yi < _box_count[1];
			++yi, y += _box_size[1] + _gap[1], x += 0.5f * (_box_size[0] + _gap[0])
		) {
			scalar cx = x;
			for (int xi = 0; xi + yi < _box_count[0]; ++xi, cx += _box_size[0] + _gap[0]) {
				lotus::physics::body_state state = lotus::uninitialized;
				if (_rotate_90) {
					state = lotus::physics::body_state::stationary_at(
						vec3(0.0f, y, cx),
						lotus::quat::from_normalized_axis_angle(vec3(0.0f, 1.0f, 0.0f), 0.5f * lotus::physics::pi)
					);
				} else {
					state = lotus::physics::body_state::stationary_at(
						vec3(cx, y, 0.0f), uquats::identity()
					);
				}
				_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
					_box_shape, material,
					_fix_first_row && yi == 0 ? lotus::physics::body_properties::kinematic() : box_props,
					state
				)));
			}
		}
	}

	void gui() override {
		ImGui::SliderInt2("Box Count", _box_count, 1, 20);
		ImGui::SliderFloat3("Box Size", _box_size, 0.0f, 2.0f, "%.1f");
		ImGui::SliderFloat2("Gap", _gap, 0.0f, 0.1f);
		ImGui::Checkbox("Rotate 90 Degrees", &_rotate_90);
		ImGui::Checkbox("Fix First Row", &_fix_first_row);
		ImGui::Checkbox("Add Walls", &_add_walls);

		ImGui::Separator();
		ImGui::SliderFloat("Static Friction", &_static_friction, 0.0f, 1.0f);
		ImGui::SliderFloat("Dynamic Friction", &_dynamic_friction, 0.0f, 1.0f);
		ImGui::SliderFloat("Restitution", &_restitution, 0.0f, 1.0f);
		ImGui::SliderFloat("Box Density", &_density, 0.0f, 100.0f);

		ImGui::Separator();
		if (ImGui::Button("Shoot Box")) {
			_shoot_box();
		}
		test::gui();
	}

	void on_key_down(lotus::system::window_events::key_down &kd) override {
		if (kd.key_code == lotus::system::key::space) {
			_shoot_box();
		}
	}

	inline static std::string get_name() {
		return "Box Stack Test";
	}
protected:
	std::deque<lotus::physics::body> _bodies;
	lotus::physics::world _world;
	//lotus::physics::xpbd::solver _solver;
	lotus::physics::sequential_impulse::solver _solver;
	debug_render _render;

	bool _rotate_90 = false;
	bool _fix_first_row = false;
	bool _add_walls = false;

	float _static_friction = 0.4f;
	float _dynamic_friction = 0.35f;
	float _restitution = 0.0f;

	float _density = 1.0f;
	float _box_size[3]{ 1.0f, 0.2f, 0.6f };
	float _gap[2]{ 0.02f, 0.02f };
	int _box_count[2]{ 5, 3 };

	lotus::collision::shape _plane_shape;
	lotus::collision::shape _box_shape;
	lotus::collision::shape _bullet_shape;
	lotus::physics::body_properties _bullet_properties = lotus::uninitialized;

	void _shoot_box() {
		auto material = lotus::physics::material_properties(_static_friction, _dynamic_friction, _restitution);
		_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
			_bullet_shape,
			material,
			_bullet_properties,
			lotus::physics::body_state::from_position_velocity(
				lotus::physics::body_position::at(
					_get_test_context().camera_params.position,
					uquats::identity()
				),
				lotus::physics::body_velocity::from_linear_angular(
					_get_test_context().camera.unit_forward * 50.0f,
					lotus::zero
				)
			)
		)));
	}
};
