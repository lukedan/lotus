#pragma once

#include <lotus/physics/sequential_impulse/solver.h>
#include <lotus/physics/xpbd/solver.h>
#include <lotus/physics/world.h>

#include "../physics_test.h"

class box_stack_test : public physics_test {
public:
	explicit box_stack_test(const test_context &tctx) : physics_test(tctx) {
	}

	void soft_reset() override {
		physics_test::soft_reset();

		_bodies.clear();
		_world.gravity = vec3(0.0f, -9.8f, 0.0f);

		_plane_shape = lotus::collision::shape::create(lotus::collision::shapes::plane());

		lotus::physics::body_properties box_props = lotus::uninitialized;
		{
			auto [box_poly, box_poly_props] = create_box_shape(vec3(_box_size[0], _box_size[1], _box_size[2]));
			_box_shape = lotus::collision::shape::create(std::move(box_poly));
			box_props = box_poly_props.get_body_properties(_density);
		}

		{
			auto [platform_poly, platform_poly_props] = create_box_shape(vec3(10.0f, 0.1f, 10.0f));
			_platform_shape = lotus::collision::shape::create(std::move(platform_poly));
		}

		auto material = lotus::physics::material_properties(
			_static_friction, _dynamic_friction, _restitution
		);

		if (_use_platform) {
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_platform_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					vec3(0.0f, -0.05f, 0.0f), uquats::identity()
				)
			)));
		} else {
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_plane_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					lotus::zero, lotus::quat::from_normalized_axis_angle(vec3(1.0f, 0.0f, 0.0f), -0.5f * lotus::physics::pi)
				)
			)));
		}
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

		scalar x = -(_box_size[0] + _gap[0]) * (static_cast<f32>(_box_count[0]) - 1.0f) / 2.0f;
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
		ImGui::SliderInt2("Box Count", _box_count, 1, 50);
		ImGui::SliderFloat3("Box Size", _box_size, 0.0f, 2.0f, "%.1f");
		ImGui::SliderFloat2("Gap", _gap, 0.0f, 2.0f);
		ImGui::Checkbox("Rotate 90 Degrees", &_rotate_90);
		ImGui::Checkbox("Fix First Row", &_fix_first_row);
		ImGui::Checkbox("Use Platform", &_use_platform);
		ImGui::Checkbox("Add Walls", &_add_walls);

		ImGui::Separator();
		ImGui::SliderFloat("Static Friction", &_static_friction, 0.0f, 1.0f);
		ImGui::SliderFloat("Dynamic Friction", &_dynamic_friction, 0.0f, 1.0f);
		ImGui::SliderFloat("Restitution", &_restitution, 0.0f, 1.0f);
		ImGui::SliderFloat("Box Density", &_density, 0.0f, 100.0f);

		ImGui::Separator();
		physics_test::gui();
	}

	static std::string get_name() {
		return "Box Stack Test";
	}
	static test_category get_category() {
		return test_category::rigid_body_physics;
	}
protected:
	bool _rotate_90 = false;
	bool _fix_first_row = false;
	bool _use_platform = false;
	bool _add_walls = false;

	f32 _static_friction = 0.4f;
	f32 _dynamic_friction = 0.35f;
	f32 _restitution = 0.0f;

	f32 _density = 1.0f;
	f32 _box_size[3]{ 1.0f, 1.0f, 1.0f };
	f32 _gap[2]{ 0.02f, 0.02f };
	int _box_count[2]{ 20, 10 };

	lotus::collision::shape _plane_shape;
	lotus::collision::shape _box_shape;
	lotus::collision::shape _platform_shape;
};
