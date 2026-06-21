#pragma once

#include "test.h"

class physics_test : public test {
public:
	explicit physics_test(const test_context &tctx) : test(tctx) {
	}

	void timestep(scalar dt, u32 iters) override {
		_solver.timestep(dt, iters);
	}

	void render(
		lotus::renderer::context &ctx, lotus::renderer::context::queue &q,
		lotus::renderer::constant_uploader &uploader,
		lotus::renderer::recorded_resources::swap_chain swap_chain, cvec2u32 size
	) override {
		_render.draw_system(_solver);
		_render.flush(ctx, q, uploader, swap_chain, size);
	}

	void soft_reset() override {
		_bodies.clear();
		_world = lotus::physics::world();
		_solver = decltype(_solver)();
		_solver.physics_world = &_world;

		_render = debug_render();
		_render.ctx = &_get_test_context();

		{
			auto [bullet_poly, bullet_poly_props] = create_box_shape(vec3::filled(_bullet_size));
			_bullet_shape = lotus::collision::shape::create(std::move(bullet_poly));
			_bullet_properties = bullet_poly_props.get_body_properties(_bullet_density);
		}
	}

	void gui() override {
		if (ImGui::Button("Shoot")) {
			_shoot_bullet();
		}
	}

	void on_key_down(lotus::system::window_events::key_down &kd) override {
		if (kd.key_code == lotus::system::key::space) {
			_shoot_bullet();
		}
	}
protected:
	lotus::physics::world _world;
	//lotus::physics::xpbd::solver _solver;
	//lotus::physics::sequential_impulse::solver _solver;
	lotus::physics::avbd::solver _solver;
	debug_render _render;
	std::deque<lotus::physics::body> _bodies;

	void _shoot_bullet() {
		auto material = lotus::physics::material_properties(_bullet_static_friction, _bullet_dynamic_friction, _bullet_restitution);
		_world.add_body(_bodies.emplace_back(lotus::physics::body::create(
			_bullet_shape,
			material,
			_bullet_properties,
			lotus::physics::body_state::from_position_velocity(
				_get_test_context().camera_params.position,
				uquats::identity(),
				_get_test_context().camera.unit_forward * _bullet_velocity
			)
		)));
	}
private:
	lotus::collision::shape _bullet_shape;
	lotus::physics::body_properties _bullet_properties = lotus::uninitialized;
	scalar _bullet_size = 1.0f;
	scalar _bullet_velocity = 50.0f;
	scalar _bullet_density = 1.0f;
	scalar _bullet_static_friction = 0.0f;
	scalar _bullet_dynamic_friction = 0.0f;
	scalar _bullet_restitution = 0.0f;
};
