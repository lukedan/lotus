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

		auto &plane = _engine.shapes.emplace_back(pbd::shape::create(pbd::shapes::plane()));

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

		auto &bullet_shape = _engine.shapes.emplace_front();
		_bullet_shape = _engine.shapes.begin();
		auto &bullet = bullet_shape.value.emplace<pbd::shapes::polyhedron>();
		pbd::cvec3d half_bullet_size(0.05, 0.05, 0.05);
		bullet.vertices.emplace_back(half_bullet_size[0], half_bullet_size[1], half_bullet_size[2]);
		bullet.vertices.emplace_back(half_bullet_size[0], half_bullet_size[1], -half_bullet_size[2]);
		bullet.vertices.emplace_back(half_bullet_size[0], -half_bullet_size[1], half_bullet_size[2]);
		bullet.vertices.emplace_back(half_bullet_size[0], -half_bullet_size[1], -half_bullet_size[2]);
		bullet.vertices.emplace_back(-half_bullet_size[0], half_bullet_size[1], half_bullet_size[2]);
		bullet.vertices.emplace_back(-half_bullet_size[0], half_bullet_size[1], -half_bullet_size[2]);
		bullet.vertices.emplace_back(-half_bullet_size[0], -half_bullet_size[1], half_bullet_size[2]);
		bullet.vertices.emplace_back(-half_bullet_size[0], -half_bullet_size[1], -half_bullet_size[2]);
		_bullet_properties = bullet.bake(10.0);

		auto material = pbd::material_properties::create(_static_friction, _dynamic_friction, _restitution);

		_engine.bodies.emplace_back(pbd::body::create(
			plane, material,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(pbd::zero, pbd::uquatd::identity())
		));
		_engine.bodies.emplace_back(pbd::body::create(
			plane, material,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(
				pbd::cvec3d(10.0, 0.0, 0.0),
				pbd::quat::from_normalized_axis_angle(pbd::cvec3d(0.0, 1.0, 0.0), -0.5 * pbd::pi)
			)
		));
		_engine.bodies.emplace_back(pbd::body::create(
			plane, material,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(
				pbd::cvec3d(-10.0, 0.0, 0.0),
				pbd::quat::from_normalized_axis_angle(pbd::cvec3d(0.0, 1.0, 0.0), 0.5 * pbd::pi)
			)
		));
		_engine.bodies.emplace_back(pbd::body::create(
			plane, material,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(
				pbd::cvec3d(0.0, 10.0, 0.0),
				pbd::quat::from_normalized_axis_angle(pbd::cvec3d(1.0, 0.0, 0.0), 0.5 * pbd::pi)
			)
		));
		_engine.bodies.emplace_back(pbd::body::create(
			plane, material,
			pbd::body_properties::kinematic(),
			pbd::body_state::stationary_at(
				pbd::cvec3d(0.0, -10.0, 0.0),
				pbd::quat::from_normalized_axis_angle(pbd::cvec3d(1.0, 0.0, 0.0), -0.5 * pbd::pi)
			)
		));

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
				_engine.bodies.emplace_back(pbd::body::create(
					box_shape, material,
					_fix_first_row && yi == 0 ? pbd::body_properties::kinematic() : box_props,
					state
				));
			}
		}

		if (_inverse_list) {
			_engine.bodies.reverse();
		}
	}

	void gui() override {
		ImGui::SliderInt2("Box Count", _box_count, 1, 10);
		ImGui::SliderFloat3("Box Size", _box_size, 0.0f, 2.0f, "%.1f");
		ImGui::SliderFloat2("Gap", _gap, 0.0f, 0.1f);
		ImGui::SliderFloat("Box Density", &_density, 0.0f, 100.0f);
		ImGui::Checkbox("Rotate 90 Degrees", &_rotate_90);
		ImGui::Checkbox("Inverse Body List", &_inverse_list);
		ImGui::Checkbox("Fix First Row", &_fix_first_row);
		if (ImGui::Button("Shoot Box")) {
			auto material = pbd::material_properties::create(_static_friction, _dynamic_friction, _restitution);
			_engine.bodies.emplace_back(pbd::body::create(
				*_bullet_shape,
				material,
				_bullet_properties,
				pbd::body_state::at(
					camera_params.position, pbd::uquatd::identity(), camera.unit_forward * 50.0, pbd::zero
				)
			));
		}
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
	bool _fix_first_row = false;

	float _static_friction = 0.4;
	float _dynamic_friction = 0.35;
	float _restitution = 0.0;

	float _density = 1.0f;
	float _box_size[3]{ 1.0f, 0.6f, 0.2f };
	float _gap[2]{ 0.02f, 0.02f };
	int _box_count[2]{ 5, 3 };

	std::deque<pbd::shape>::iterator _bullet_shape;
	pbd::body_properties _bullet_properties = pbd::uninitialized;
};
