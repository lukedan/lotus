#pragma once

#include "../physics_test.h"

class pin_test : public physics_test {
public:
	explicit pin_test(const test_context &tctx) : physics_test(tctx) {
	}

	void soft_reset() override {
		physics_test::soft_reset();

		_world.gravity = vec3(0.0f, -9.8f, 0.0f);

		_plane_shape = lotus::collision::shape::create(lotus::collision::shapes::plane());

		lotus::physics::body_properties box_props = lotus::uninitialized;
		{
			auto [box_poly, box_poly_props] = create_box_shape(vec3::filled(_box_size));
			_box_shape = lotus::collision::shape::create(std::move(box_poly));
			box_props = box_poly_props.get_body_properties(1.0f);
		}

		const lotus::physics::material_properties material(0.0f, 0.0f, 0.0f);

		const auto add_pin = [&](lotus::physics::body *body1, lotus::physics::body *body2, vec3 point_ws) {
			lotus::physics::avbd::constraints::pin &pin = _solver.pins.emplace_back();
			pin.body1 = body1;
			pin.body2 = body2;
			pin.local_position1 = body1 ? body1->state.position.global_to_local(point_ws) : point_ws;
			pin.local_position2 = body2 ? body2->state.position.global_to_local(point_ws) : point_ws;
		};

		lotus::physics::body *prev_body = nullptr;
		for (int i = 0; i < _num_boxes; ++i) {
			lotus::physics::body &body = _bodies.emplace_back(lotus::physics::body::create(
				_box_shape, material, box_props,
				lotus::physics::body_state::stationary_at(vec3(_gap * (static_cast<f32>(i) + 1), 0.0f, 0.0f), uquats::identity())
			));
			_world.add_body(body);

			if (_double_pin) {
				const vec3 point1_ws = vec3(_gap * (static_cast<f32>(i) + 0.5f), 0.0f, _box_size / 6.0f);
				const vec3 point2_ws = vec3(_gap * (static_cast<f32>(i) + 0.5f), 0.0f, -_box_size / 6.0f);
				add_pin(prev_body, &body, point1_ws);
				add_pin(prev_body, &body, point2_ws);
			} else {
				const vec3 point_ws = vec3(_gap * (static_cast<f32>(i) + 0.5f), 0.0f, 0.0f);
				add_pin(prev_body, &body, point_ws);
			}
			prev_body = &body;
		}

		_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
			_plane_shape, material,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(
				vec3(0.0f, -_height, 0.0f), lotus::quat::from_normalized_axis_angle(vec3(1.0f, 0.0f, 0.0f), -0.5f * lotus::physics::pi)
			)
		)));
	}

	void gui() override {
		ImGui::SliderInt("Num Boxes", &_num_boxes, 1, 10);
		ImGui::SliderFloat("Box Size", &_box_size, 0.1f, 2.0f);
		ImGui::SliderFloat("Gap", &_gap, 1.0f, 10.0f);
		ImGui::SliderFloat("Height", &_height, 2.0f, 150.0f);
		ImGui::Checkbox("Double Pin", &_double_pin);

		ImGui::Separator();
		physics_test::gui();
	}

	static std::string get_name() {
		return "Pin Test";
	}
	static test_category get_category() {
		return test_category::rigid_body_physics;
	}
private:
	lotus::collision::shape _box_shape;
	lotus::collision::shape _plane_shape;

	int _num_boxes = 3;
	f32 _box_size = 1.0f;
	f32 _gap = 5.0f;
	f32 _height = 5.0f;
	bool _double_pin = false;
};
