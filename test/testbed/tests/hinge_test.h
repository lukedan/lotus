#pragma once

#include "../physics_test.h"

class hinge_test : public physics_test {
public:
	explicit hinge_test(test_context &tctx) : physics_test(tctx) {
	}

	void soft_reset() override {
		physics_test::soft_reset();

		_world.gravity = vec3(0.0f, -9.8f, 0.0f);

		_plane_shape = lotus::collision::shape::create(lotus::collision::shapes::plane());

		lotus::physics::body_properties door_props = lotus::uninitialized;
		{
			auto [door_poly, door_poly_props] = create_box_shape({ _door_size[0], _door_size[1], _door_size[2] });
			_door_shape = lotus::collision::shape::create(std::move(door_poly));
			door_props = door_poly_props.get_body_properties(1.0f);
		}

		const lotus::physics::material_properties material(0.0f, 0.0f, 0.0f);

		lotus::physics::body &door = _bodies.emplace_back(lotus::physics::body::create(
			_door_shape, material, door_props,
			lotus::physics::body_state::stationary_at(vec3(0.0f, 0.5f * _door_size[1] + _height, 0.0f), uquats::identity())
		));
		_world.add_body(door);

		{
			const vec3 pin_pos = vec3(0.5f * _door_size[0], 0.5f * _door_size[1] + _height, 0.0f);
			lotus::physics::constraints::pin &pin = _world.pins.emplace_back(lotus::zero);
			pin.body1 = &door;
			pin.local_position1 = door.state.position.global_to_local(pin_pos);
			pin.local_position2 = pin_pos;
		}
		{
			lotus::physics::constraints::hinge &hinge = _world.hinges.emplace_back(lotus::zero);
			hinge.body1 = &door;
			hinge.local_axis1 = { 0.0f, 1.0f, 0.0f };
			hinge.local_axis2 = { 0.0f, 1.0f, 0.0f };
		}

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
					vec3(5.0f, 0.0f, 0.0f),
					lotus::quat::from_normalized_axis_angle(vec3(0.0f, 1.0f, 0.0f), -0.5f * lotus::physics::pi)
				)
			)));
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_plane_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					vec3(-5.0f, 0.0f, 0.0f),
					lotus::quat::from_normalized_axis_angle(vec3(0.0f, 1.0f, 0.0f), 0.5f * lotus::physics::pi)
				)
			)));
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_plane_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					vec3(0.0f, 0.0f, 5.0f),
					lotus::quat::from_normalized_axis_angle(vec3(0.0f, 1.0f, 0.0f), lotus::physics::pi)
				)
			)));
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				_plane_shape, material,
				lotus::physics::body_properties::kinematic(),
				lotus::physics::body_state::stationary_at(
					vec3(0.0f, 0.0f, -5.0f), uquats::identity()
				)
			)));
		}
	}

	void gui() override {
		physics_test::gui();

		ImGui::Separator();
		ImGui::SliderFloat3("Door Size", _door_size, 0.01f, 3.0f);
		ImGui::SliderFloat("Height", &_height, 0.0f, 1.0f);
		ImGui::Checkbox("Add Walls", &_add_walls);
	}

	static std::string get_name() {
		return "Hinge Test";
	}
	static test_category get_category() {
		return test_category::rigid_body_physics;
	}
private:
	lotus::collision::shape _door_shape;
	lotus::collision::shape _plane_shape;

	f32 _door_size[3] = { 1.0f, 2.3f, 0.1f };
	f32 _height = 0.1f;
	bool _add_walls = false;
};
