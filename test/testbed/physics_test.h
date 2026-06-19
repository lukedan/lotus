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
		lotus::renderer::recorded_resources::swap_chain swap_chain, lotus::renderer::recorded_resources::image2d_view depth_stencil, lotus::cvec2u32 size
	) override {
		_render.draw_system(_solver);
		_render.flush(ctx, q, uploader, swap_chain, depth_stencil, size);
	}

	void soft_reset() override {
		_world = lotus::physics::world();
		_solver = decltype(_solver)();
		_solver.physics_world = &_world;

		_render = debug_render();
		_render.ctx = &_get_test_context();
	}
protected:
	lotus::physics::world _world;
	//lotus::physics::xpbd::solver _solver;
	//lotus::physics::sequential_impulse::solver _solver;
	lotus::physics::avbd::solver _solver;
	debug_render _render;
};
