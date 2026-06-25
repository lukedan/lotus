#pragma once

#include <lotus/physics/sequential_impulse/solver.h>
#include <lotus/physics/world.h>

#include "../physics_test.h"

class angular_momentum_test : public physics_test {
public:
	explicit angular_momentum_test(test_context &tctx) : physics_test(tctx) {
	}

	void soft_reset() override {
		physics_test::soft_reset();

		_bodies.clear();

		lotus::physics::body_properties box_props = lotus::uninitialized;
		{
			auto [box_poly, box_poly_props] = create_box_shape(vec3(_box_size[0], _box_size[1], _box_size[2]));
			_box_shape = lotus::collision::shape::create(std::move(box_poly));
			box_props = box_poly_props.get_body_properties(1.0f);
			for (usize y = 0; y < 3; ++y) {
				for (usize x = 0; x < 3; ++x) {
					if (x == y) {
						continue;
					}
					box_props.inertia(y, x) = 0.0f;
					box_props.inverse_inertia(y, x) = 0.0f;
				}
			}
		}

		auto material = lotus::physics::material_properties(0.0f, 0.0f, 0.0f);

		{
			lotus::physics::body &b = _bodies.emplace_back(lotus::physics::body::create(
				_box_shape, material, box_props,
				lotus::physics::body_state::stationary_at(vec3(-2.0f, 0.0f, 0.0f), uquats::identity())
			));
			b.state.velocity.angular = vec3(_angular_velocity, _perturbation, _perturbation);
			_world.add_body(b);
		}

		{
			lotus::physics::body &b = _bodies.emplace_back(lotus::physics::body::create(
				_box_shape, material, box_props,
				lotus::physics::body_state::stationary_at(vec3(0.0f, 0.0f, 0.0f), uquats::identity())
			));
			b.state.velocity.angular = vec3(_perturbation, _angular_velocity, _perturbation);
			_world.add_body(b);
		}

		{
			lotus::physics::body &b = _bodies.emplace_back(lotus::physics::body::create(
				_box_shape, material, box_props,
				lotus::physics::body_state::stationary_at(vec3(2.0f, 0.0f, 0.0f), uquats::identity())
			));
			b.state.velocity.angular = vec3(_perturbation, _perturbation, _angular_velocity);
			_world.add_body(b);
		}
	}

	static std::string get_name() {
		return "Angular Momentum Test";
	}
	static test_category get_category() {
		return test_category::rigid_body_physics;
	}
private:
	f32 _box_size[3]{ 0.2f, 0.5f, 0.8f };
	f32 _angular_velocity = 10.0f;
	f32 _perturbation = 0.01f;

	lotus::collision::shape _box_shape;
};
