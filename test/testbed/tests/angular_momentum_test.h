#pragma once

#include <lotus/physics/sequential_impulse/solver.h>
#include <lotus/physics/world.h>

#include "test.h"

class angular_momentum_test : public test {
public:
	explicit angular_momentum_test(const test_context &tctx) : test(tctx) {
		soft_reset();
	}

	void timestep(scalar dt, u32 iters) override {
		_solver.timestep(dt, iters);
	}

	void render(
		lotus::renderer::context &ctx, lotus::renderer::context::queue &q,
		lotus::renderer::constant_uploader &uploader,
		lotus::renderer::image2d_color color, lotus::renderer::image2d_depth_stencil depth, lotus::cvec2u32 size
	) override {
		_render.draw_system(_solver);
		_render.flush(ctx, q, uploader, color, depth, size);
	}

	void soft_reset() override {
		_bodies.clear();
		_world = lotus::physics::world();
		_solver = decltype(_solver)();
		_solver.physics_world = &_world;

		_render = debug_render();
		_render.ctx = &_get_test_context();

		lotus::physics::body_properties box_props = lotus::uninitialized;
		{
			std::vector<vec3> box_verts;
			vec3 half_size = vec3(_box_size[0], _box_size[1], _box_size[2]) / 2.0f;
			box_verts.emplace_back( half_size[0],  half_size[1],  half_size[2]);
			box_verts.emplace_back( half_size[0],  half_size[1], -half_size[2]);
			box_verts.emplace_back( half_size[0], -half_size[1],  half_size[2]);
			box_verts.emplace_back( half_size[0], -half_size[1], -half_size[2]);
			box_verts.emplace_back(-half_size[0],  half_size[1],  half_size[2]);
			box_verts.emplace_back(-half_size[0],  half_size[1], -half_size[2]);
			box_verts.emplace_back(-half_size[0], -half_size[1],  half_size[2]);
			box_verts.emplace_back(-half_size[0], -half_size[1], -half_size[2]);
			auto [box_poly, box_poly_props] = lotus::collision::shapes::convex_polyhedron::bake(box_verts);
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

		auto i = box_props.inverse_inertia * box_props.inertia;

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

	void gui() override {
		test::gui();
	}

	static std::string get_name() {
		return "Angular Momentum Test";
	}
private:
	std::deque<lotus::physics::body> _bodies;
	lotus::physics::world _world;
	//lotus::physics::sequential_impulse::solver _solver;
	lotus::physics::avbd::solver _solver;
	debug_render _render;

	f32 _box_size[3]{ 0.2f, 0.5f, 0.8f };
	f32 _angular_velocity = 10.0f;
	f32 _perturbation = 0.01f;

	lotus::collision::shape _box_shape;
};
