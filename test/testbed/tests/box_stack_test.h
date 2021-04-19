#pragma once

#include <pbd/engine.h>

#include "test.h"

class box_stack_test : public test {
public:
	box_stack_test() {
		soft_reset();
	}

	void timestep(double dt, std::size_t iters) override {
		_engine.timestep(dt, iters);
	}

	void render(const draw_options &options) override {
		_render.draw(options);
	}

	void soft_reset() override {
		_engine = pbd::engine();
		_engine.gravity = pbd::cvec3d(0.0, 0.0, -9.8);

		_render = debug_render();
		_render.engine = &_engine;

		auto &plane = _engine.shapes.emplace_back(pbd::shapes::plane());

		auto &clip_visuals = _render.body_visuals.emplace_back();
		clip_visuals.color = { 1.0f, 0.6f, 0.2f, 0.4f };

		auto &box_shape = _engine.shapes.emplace_back();
		auto &box = box_shape.value.emplace<pbd::shapes::polyhedron>();
		pbd::cvec3d half_size(_box_size[0], _box_size[1], _box_size[2]);
		half_size *= 0.5;
		box.vertices.emplace_back( half_size[0],  half_size[1],  half_size[2]);
		box.vertices.emplace_back( half_size[0],  half_size[1], -half_size[2]);
		box.vertices.emplace_back( half_size[0], -half_size[1],  half_size[2]);
		box.vertices.emplace_back( half_size[0], -half_size[1], -half_size[2]);
		box.vertices.emplace_back(-half_size[0],  half_size[1],  half_size[2]);
		box.vertices.emplace_back(-half_size[0],  half_size[1], -half_size[2]);
		box.vertices.emplace_back(-half_size[0], -half_size[1],  half_size[2]);
		box.vertices.emplace_back(-half_size[0], -half_size[1], -half_size[2]);
		auto box_props = box.bake(1.0);

		_engine.bodies.emplace_back(pbd::body::create(
			plane,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(pbd::zero, pbd::uquatd::identity())
		));
		_engine.bodies.emplace_back(pbd::body::create(
			plane,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(
				pbd::cvec3d(10.0, 0.0, 0.0),
				pbd::quat::from_normalized_axis_angle(pbd::cvec3d(0.0, 1.0, 0.0), -0.5 * pbd::pi)
			)
		)).user_data = &clip_visuals;
		_engine.bodies.emplace_back(pbd::body::create(
			plane,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(
				pbd::cvec3d(-10.0, 0.0, 0.0),
				pbd::quat::from_normalized_axis_angle(pbd::cvec3d(0.0, 1.0, 0.0), 0.5 * pbd::pi)
			)
		)).user_data = &clip_visuals;
		_engine.bodies.emplace_back(pbd::body::create(
			plane,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(
				pbd::cvec3d(0.0, 10.0, 0.0),
				pbd::quat::from_normalized_axis_angle(pbd::cvec3d(1.0, 0.0, 0.0), 0.5 * pbd::pi)
			)
		)).user_data = &clip_visuals;
		_engine.bodies.emplace_back(pbd::body::create(
			plane,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(
				pbd::cvec3d(0.0, -10.0, 0.0),
				pbd::quat::from_normalized_axis_angle(pbd::cvec3d(1.0, 0.0, 0.0), -0.5 * pbd::pi)
			)
		)).user_data = &clip_visuals;

		double x = -(_box_size[0] + _gap[0]) * (_box_count[0] - 1) / 2.0;
		double z = 0.5 * _box_size[2] + _gap[1];
		for (
			int yi = 0;
			yi < _box_count[1];
			++yi, z += _box_size[2] + _gap[1], x += 0.5 * (_box_size[0] + _gap[0])
		) {
			double cx = x;
			for (int xi = 0; xi + yi < _box_count[0]; ++xi, cx += _box_size[0] + _gap[0]) {
				pbd::body_state state = pbd::uninitialized;
				if (_rotate_90) {
					state = pbd::body_state::stationary_at(
						pbd::cvec3d(0.0, cx, z),
						pbd::quat::from_normalized_axis_angle(pbd::cvec3d(0.0, 0.0, 1.0), 0.5 * pbd::pi)
					);
				} else {
					state = pbd::body_state::stationary_at(pbd::cvec3d(cx, 0.0, z), pbd::uquatd::identity());
				}
				_engine.bodies.emplace_back(pbd::body::create(box_shape, box_props, state));
			}
		}

		if (_inverse_list) {
			_engine.bodies.reverse();
		}
	}

	void gui() {
		ImGui::SliderInt2("Box Count", _box_count, 1, 10);
		ImGui::SliderFloat3("Box Size", _box_size, 0.0f, 2.0f);
		ImGui::SliderFloat2("Gap", _gap, 0.0f, 0.1f);
		ImGui::SliderFloat("Box Density", &_density, 0.0f, 100.0f);
		ImGui::Checkbox("Rotate 90 Degrees", &_rotate_90);
		ImGui::Checkbox("Inverse Body List", &_inverse_list);
		test::gui();
	}

	inline static std::string get_name() {
		return "Box Stack Test";
	}
protected:
	pbd::engine _engine;
	debug_render _render;

	bool _rotate_90 = false;
	bool _inverse_list = false;

	float _density = 1.0f;
	float _box_size[3]{ 1.0f, 0.6f, 0.2f };
	float _gap[2]{ 0.02f, 0.02f };
	int _box_count[2]{ 5, 3 };
};
