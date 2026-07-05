#pragma once

#include "../physics_test.h"

class car_test : public physics_test {
public:
	enum class wheel_type {
		aligned_poly,
		staggered_poly,
		cylinder,

		count
	};
	constexpr static const char *wheel_type_names[] = {
		"Aligned Polyhedron",
		"Staggered Polyhedron",
		"Cylinder",
	};
	struct wheel_info {
		wheel_info(std::nullptr_t) {
		}
		wheel_info(u32 s, u32 i, u32 hi, lotus::physics::body &b) : side(s), index(i), hinge_index(hi), body(&b) {
		}

		u32 side = 0;
		u32 index = 0;
		u32 hinge_index = 0;
		lotus::physics::body *body = nullptr;
	};

	explicit car_test(test_context &tctx) : physics_test(tctx) {
	}

	void soft_reset() override {
		physics_test::soft_reset();

		_wheels.clear();
		_world.gravity = vec3(0.0f, -9.8f, 0.0f);

		_plane_shape = lotus::collision::shape::create(lotus::collision::shapes::plane());

		const lotus::physics::material_properties body_material(0.2f, 0.2f, 0.0f);
		const lotus::physics::material_properties wheel_material(_wheel_static_friction, _wheel_dynamic_friction, 0.2f);
		const lotus::physics::material_properties ground_material(0.5f, 0.4f, 0.0f);

		lotus::physics::body_properties arm_props = lotus::uninitialized;
		{
			auto [arm_poly, arm_poly_props] = create_box_shape({ _arm_length, 0.1f, 0.1f });
			_arm_shape = lotus::collision::shape::create(std::move(arm_poly));
			arm_props = arm_poly_props.get_body_properties(_body_density);
		}

		lotus::physics::body_properties body_props = lotus::uninitialized;
		{
			auto [body_poly, body_poly_props] = create_box_shape({ _body_size[0], _body_size[1], _body_size[2] });
			_body_shape = lotus::collision::shape::create(std::move(body_poly));
			body_props = body_poly_props.get_body_properties(_body_density);
		}

		lotus::physics::body_properties wheel_props = lotus::uninitialized;
		{
			const auto wtype = static_cast<wheel_type>(_wheel_type);
			std::vector<vec3> _wheel_verts;
			const scalar angle = 2.0f * lotus::physics::pi / _wheel_subdivision;
			for (int i = 0; i < _wheel_subdivision; ++i) {
				const vec2 dir(std::cos(angle * i), std::sin(angle * i));
				_wheel_verts.emplace_back(0.5f * _wheel_width, _wheel_radius * dir);
				if (wtype == wheel_type::aligned_poly) {
					_wheel_verts.emplace_back(-0.5f * _wheel_width, _wheel_radius * dir);
				} else if (wtype == wheel_type::staggered_poly) {
					const vec2 new_dir(std::cos(angle * (i + 0.5f)), std::sin(angle * (i + 0.5f)));
					_wheel_verts.emplace_back(-0.5f * _wheel_width, _wheel_radius * new_dir);
				}
			}
			auto [wheel_poly, wheel_poly_props] = lotus::collision::shapes::convex_polyhedron::bake(_wheel_verts);
			_wheel_shape = lotus::collision::shape::create(std::move(wheel_poly));
			wheel_props = wheel_poly_props.get_body_properties(_wheel_density);
		}

		_body = &_bodies.emplace_back(lotus::physics::body::create(
			_body_shape, body_material, body_props, lotus::physics::body_state::stationary_at(
				vec3(0.0f, _initial_pos_y, 0.0f), uquats::identity()
			)
		));
		_world.add_body(*_body);

		for (u32 side = 0; side < 2; ++side) {
			const f32 side_sign = side == 0 ? 1.0f : -1.0f;
			for (u32 index = 0; index < 2; ++index) {
				const f32 z = -0.5f * _wheel_spacing + index * _wheel_spacing;

				// create arm
				lotus::physics::body &arm = _bodies.emplace_back(lotus::physics::body::create(
					_arm_shape, body_material, arm_props, lotus::physics::body_state::stationary_at(
						vec3(side_sign * 0.5f * (_width - _arm_length), _initial_pos_y - 0.5f * _body_size[1] - _body_lift, z),
						uquats::identity()
					)
				));
				_world.add_body(arm);

				{ // attach arm to body
					const vec3 pin_pos = arm.state.position.position - vec3(side_sign * 0.5f * _arm_length, 0.0f, 0.0f);

					lotus::physics::constraints::pin &pin = _world.pins.emplace_back(lotus::zero);
					pin.body1 = _body;
					pin.body2 = &arm;
					pin.local_position1 = pin.body1->state.position.global_to_local(pin_pos);
					pin.local_position2 = pin.body2->state.position.global_to_local(pin_pos);
					pin.disable_collision = true;

					lotus::physics::constraints::hinge &hinge = _world.hinges.emplace_back(lotus::zero);
					hinge.body1 = _body;
					hinge.body2 = &arm;
					hinge.local_axis1 = vec3(0.0f, 0.0f, 1.0f);
					hinge.local_axis2 = vec3(0.0f, 0.0f, 1.0f);

					const vec3 spring_pos = vec3(side_sign * (0.5f * _width - _spring_position), _initial_pos_y - 0.5f * _body_size[1] - _body_lift, z);
					lotus::physics::constraints::spring &spring = _world.springs.emplace_back(lotus::zero);
					spring.body1 = _body;
					spring.body2 = &arm;
					spring.local_position1 = spring.body1->state.position.global_to_local(spring_pos + vec3(0.0f, _spring_length_initial, 0.0f));
					spring.local_position2 = spring.body2->state.position.global_to_local(spring_pos);
					spring.initial_length = _spring_length_full;
					spring.compressed_stiffness = _suspension_stiffness;
					spring.stretched_stiffness = _suspension_stiffness;
				}

				// create wheel
				lotus::physics::body &wheel = _bodies.emplace_back(lotus::physics::body::create(
					_wheel_shape, wheel_material, wheel_props, lotus::physics::body_state::stationary_at(
						vec3(side_sign * 0.5f * _width, _initial_pos_y - 0.5f * _body_size[1] - _body_lift, z),
						uquats::identity()
					)
				));
				_world.add_body(wheel);
				_wheels.emplace_back(side, index, _world.hinges.size(), wheel);

				{ // attach wheel to body
					const vec3 pin_pos = wheel.state.position.position - vec3(side_sign * _wheel_pivot, 0.0f, 0.0f);

					lotus::physics::constraints::pin &pin = _world.pins.emplace_back(lotus::zero);
					pin.body1 = &arm;
					pin.body2 = &wheel;
					pin.local_position1 = pin.body1->state.position.global_to_local(pin_pos);
					pin.local_position2 = pin.body2->state.position.global_to_local(pin_pos);
					pin.disable_collision = true;

					lotus::physics::constraints::hinge &hinge = _world.hinges.emplace_back(lotus::zero);
					hinge.body1 = &arm;
					hinge.body2 = &wheel;
					hinge.local_axis1 = vec3(1.0f, 0.0f, 0.0f);
					hinge.local_axis2 = vec3(1.0f, 0.0f, 0.0f);
				}
			}
		}

		_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
			_plane_shape, ground_material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(
				lotus::zero, lotus::quat::from_normalized_axis_angle(vec3(1.0f, 0.0f, 0.0f), -0.5f * lotus::physics::pi)
			)
		)));
	}

	void timestep(scalar dt) override {
		{
			scalar torque = 0.0f;
			if (_accelerating) {
				torque += _power * dt;
			}
			if (_decelerating) {
				torque -= _power * dt;
			}
			for (const wheel_info wheel : _wheels) {
				if (wheel.index == 0 && !_front_wheel_drive) {
					continue;
				}
				if (wheel.index == 1 && !_rear_wheel_drive) {
					continue;
				}
				wheel.body->applied_torque += wheel.body->state.position.orientation.rotate(vec3(-torque, 0.0f, 0.0f));
			}
		}
		{
			scalar turn = 0.0f;
			if (_turn_left) {
				turn -= 0.2f * lotus::physics::pi;
			}
			if (_turn_right) {
				turn += 0.2f * lotus::physics::pi;
			}
			for (const wheel_info wheel : _wheels) {
				if (wheel.index != 0) {
					continue;
				}
				lotus::physics::constraints::hinge &hinge = _world.hinges[wheel.hinge_index];
				hinge.local_axis1 = vec3(std::cos(turn), 0.0f, std::sin(turn));
			}
		}

		physics_test::timestep(dt);

		if (_attach_cam) {
			test_context &tctx = _get_test_context();
			const vec3 offset = _body->state.position.position - tctx.camera_params.look_at;
			tctx.camera_params.position += offset;
			tctx.camera_params.look_at += offset;
			tctx.update_camera();
		}
	}

	void gui() override {
		physics_test::gui();

		ImGui::Separator();
		ImGui::SliderFloat("Suspension Stiffness", &_suspension_stiffness, 0.0f, 10000000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Spring Length Initial", &_spring_length_initial, 0.1f, _spring_length_full);
		ImGui::SliderFloat("Spring Length Full", &_spring_length_full, 0.1f, 1.0f);
		ImGui::SliderFloat("Wheel Spacing", &_wheel_spacing, 1.0f, 10.0f);
		ImGui::SliderFloat("Width", &_width, 1.0f, 5.0f);
		ImGui::SliderFloat("Arm Length", &_arm_length, 0.0f, 2.0f);
		ImGui::SliderFloat("Body Lift", &_body_lift, -1.0f, 1.0f);
		ImGui::SliderFloat3("Body Size", _body_size, 0.0f, 10.0f);
		// TODO
		ImGui::Combo("Wheel Type", &_wheel_type, wheel_type_names, static_cast<int>(wheel_type::count));
		ImGui::SliderInt("Wheel Subdivision", &_wheel_subdivision, 3, 50);
		ImGui::SliderFloat("Wheel Static Friction", &_wheel_static_friction, 0.0f, 2.0f);
		ImGui::SliderFloat("Wheel Dynamic Friction", &_wheel_dynamic_friction, 0.0f, 2.0f);
		ImGui::SliderFloat("Power", &_power, 0.0f, 10000.0f);
		ImGui::Checkbox("Front Wheel Drive", &_front_wheel_drive);
		ImGui::Checkbox("Rear Wheel Drive", &_rear_wheel_drive);
		ImGui::Checkbox("Attach Camera To Car", &_attach_cam);

		ImGui::Text("Air Speed = %g", _body->state.velocity.linear.norm());
	}

	void on_key_down(lotus::system::window_events::key_down &key) override {
		switch (key.key_code) {
		case lotus::system::key::up:
			_accelerating = true;
			break;
		case lotus::system::key::down:
			_decelerating = true;
			break;
		case lotus::system::key::left:
			_turn_left = true;
			break;
		case lotus::system::key::right:
			_turn_right = true;
			break;
		default:
			break;
		}
		physics_test::on_key_down(key);
	}

	void on_key_up(lotus::system::window_events::key_up &key) override {
		switch (key.key_code) {
		case lotus::system::key::up:
			_accelerating = false;
			break;
		case lotus::system::key::down:
			_decelerating = false;
			break;
		case lotus::system::key::left:
			_turn_left = false;
			break;
		case lotus::system::key::right:
			_turn_right = false;
			break;
		default:
			break;
		}
		physics_test::on_key_up(key);
	}

	static std::string get_name() {
		return "Car Test";
	}
	static test_category get_category() {
		return test_category::rigid_body_physics;
	}
private:
	lotus::collision::shape _wheel_shape;
	lotus::collision::shape _arm_shape;
	lotus::collision::shape _body_shape;
	lotus::collision::shape _plane_shape;

	lotus::physics::body *_body = nullptr;
	std::vector<wheel_info> _wheels;

	f32 _body_lift = -0.2f;
	f32 _body_size[3] = { 1.2f, 0.5f, 4.0f };
	f32 _body_density = 1000.0f;
	f32 _width = 2.0f;

	f32 _arm_length = 0.8f;
	f32 _suspension_stiffness = 100000.0f;
	f32 _spring_length_initial = 0.5f;
	f32 _spring_length_full = 0.6f;
	f32 _spring_position = 0.1f;

	f32 _wheel_spacing = 2.5f;
	f32 _wheel_pivot = 0.2f;
	f32 _wheel_radius = 0.3f;
	f32 _wheel_width = 0.2f;
	int _wheel_type = static_cast<int>(wheel_type::staggered_poly);
	int _wheel_subdivision = 20;
	f32 _wheel_static_friction = 1.2f;
	f32 _wheel_dynamic_friction = 1.1f;
	f32 _wheel_density = 1000.0f;
	f32 _initial_pos_y = 1.0f;

	f32 _power = 1000.0f;
	bool _front_wheel_drive = true;
	bool _rear_wheel_drive = true;

	bool _accelerating = false;
	bool _decelerating = false;
	bool _turn_left = false;
	bool _turn_right = false;

	bool _attach_cam = true;
};
