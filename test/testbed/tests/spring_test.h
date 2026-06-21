#pragma once

#include "../physics_test.h"

class spring_test : public physics_test {
public:
	explicit spring_test(const test_context &tctx) : physics_test(tctx) {
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

		lotus::physics::body *prev_body = nullptr;
		for (int i = 0; i < _num_boxes; ++i) {
			lotus::physics::body &body = _bodies.emplace_back(lotus::physics::body::create(
				_box_shape, material, box_props,
				lotus::physics::body_state::stationary_at(vec3(_gap * (static_cast<f32>(i) + 1), 0.0f, 0.0f), uquats::identity())
			));
			_world.add_body(body);
			{
				lotus::physics::avbd::constraints::spring &spring = _solver.springs.emplace_back();
				spring.body1 = &body;
				spring.body2 = prev_body;
				spring.local_position1 = vec3(-0.5f * _box_size, 0.0f, 0.0f);
				spring.local_position2 = prev_body ? vec3(0.5f * _box_size, 0.0f, 0.0f) : lotus::zero;
				spring.initial_length = (spring.get_global_position1() - spring.get_global_position2()).norm();
				spring.compressed_stiffness = _compressed_stiffness;
				spring.stretched_stiffness = _stretched_stiffness;
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
		ImGui::SliderFloat("Compressed Stiffness", &_compressed_stiffness, 0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Stretched Stiffness", &_stretched_stiffness, 0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Height", &_height, 2.0f, 150.0f);

		ImGui::Separator();
		physics_test::gui();
	}

	static std::string get_name() {
		return "Spring Test";
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
	f32 _compressed_stiffness = 0.0f;
	f32 _stretched_stiffness = 100.0f;
	f32 _height = 5.0f;
};
