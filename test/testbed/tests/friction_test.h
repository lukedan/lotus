#include "../physics_test.h"

class friction_test : public physics_test {
public:
	explicit friction_test(test_context &tctx) : physics_test(tctx) {
	}

	void soft_reset() override {
		physics_test::soft_reset();

		_world.gravity = vec3(0.0f, -9.8f, 0.0f);

		_plane_shape = lotus::collision::shape::create(lotus::collision::shapes::plane());
		_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
			_plane_shape,
			lotus::physics::material_properties(_ground_friction, _ground_friction, 0.0f),
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(
				lotus::zero, lotus::quat::from_normalized_axis_angle(vec3(1.0f, 0.0f, 0.0f), -0.5f * lotus::physics::pi)
			)
		)));
		_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
			_plane_shape,
			lotus::physics::material_properties(_ground_friction, _ground_friction, 0.0f),
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(
				lotus::zero, lotus::quat::from_normalized_axis_angle(vec3(1.0f, 0.0f, 0.0f), -(90.0f - _angle) * lotus::physics::pi / 180.0f)
			)
		)));

		scalar pos = 0.0f;
		scalar box_rotation = _angle * lotus::physics::pi / 180.0f;
		scalar box_rotx = std::cos(box_rotation);
		scalar box_roty = std::sin(box_rotation);
		for (int i = 0; i < _box_count; ++i) {
			const vec3 box_size = vec3(_size_min) + vec3(_size_step) * i;
			auto [box_poly, box_poly_props] = create_box_shape(box_size);
			lotus::collision::shape &shape = _box_shapes.emplace_back(lotus::collision::shape::create(std::move(box_poly)));
			const lotus::physics::body_properties box_props = box_poly_props.get_body_properties(1.0f);

			const f32 friction = _box_friction_min + i * _box_friction_step;
			_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
				shape,
				lotus::physics::material_properties(friction, friction, 0.0f),
				box_props,
				lotus::physics::body_state::stationary_at(
					vec3(0.0f, box_roty, -box_rotx) * _distance +
					vec3(0.0f, box_rotx, box_roty) * 0.5f * box_size[1] +
					vec3(pos + 0.5f * box_size[0], _height, 0.0f),
					lotus::quat::from_normalized_axis_angle(vec3(1.0f, 0.0f, 0.0f), box_rotation)
				)
			)));

			pos += 1.0f + box_size[0];
		}
	}

	void gui() override {
		physics_test::gui();

		ImGui::Separator();
		ImGui::SliderFloat("Angle", &_angle, 0.0f, 90.0f);
		ImGui::SliderFloat("Distance", &_distance, 0.0f, 50.0f);
		ImGui::SliderFloat("Height", &_height, 0.0f, 10.0f);
		ImGui::SliderInt("Box Count", &_box_count, 1, 20);
		ImGui::SliderFloat("Ground Friction", &_ground_friction, 0.0f, 2.0f);
		ImGui::SliderFloat("Box Friction Min", &_box_friction_min, 0.0f, 1.0f);
		ImGui::SliderFloat("Box Friction Step", &_box_friction_step, 0.0f, 1.0f);
		ImGui::SliderFloat3("Size Min", _size_min, 0.0f, 1.0f);
		ImGui::SliderFloat3("Size Step", _size_step, -1.0f, 1.0f);
	}

	static std::string get_name() {
		return "Friction Test";
	}
	static test_category get_category() {
		return test_category::rigid_body_physics;
	}
private:
	f32 _angle = 30.0f;
	f32 _distance = 10.0f;
	f32 _height = 2.0f;
	int _box_count = 10;

	f32 _ground_friction = 1.0f;
	f32 _box_friction_min = 0.0f;
	f32 _box_friction_step = 0.1f;
	f32 _size_min[3] = { 1.0f, 1.0f, 1.0f };
	f32 _size_step[3] = { 0.0f, 0.0f, 0.0f };

	lotus::collision::shape _plane_shape;
	std::deque<lotus::collision::shape> _box_shapes;
};
