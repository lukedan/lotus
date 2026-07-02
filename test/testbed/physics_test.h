#pragma once

#include "test.h"

class physics_test : public test {
public:
	using solver_variant = std::variant<
		lotus::physics::solvers::sequential_impulse::solver,
		lotus::physics::solvers::avbd::solver,
		lotus::physics::solvers::xpbd::solver
	>;
	struct solver_creator {
		const char *name;
		void (*creator)(solver_variant&);
	};
	constexpr static std::array<solver_creator, 3> solver_creators = { {
		{ "Sequential Impulse", [](solver_variant &v) { v.emplace<lotus::physics::solvers::sequential_impulse::solver>(); } },
		{ "AVBD", [](solver_variant &v) { v.emplace<lotus::physics::solvers::avbd::solver>(); } },
		{ "XPBD", [](solver_variant &v) { v.emplace<lotus::physics::solvers::xpbd::solver>(); } },
	} };

	explicit physics_test(test_context &tctx) : test(tctx) {
	}

	void timestep(scalar dt) override {
		std::visit(
			[dt](auto &solver) {
				solver.timestep(dt);
			},
			_solver
		);
	}

	void render(
		lotus::renderer::context &ctx, lotus::renderer::context::queue &q,
		lotus::renderer::constant_uploader &uploader,
		lotus::renderer::recorded_resources::swap_chain swap_chain, cvec2u32 size
	) override {
		std::visit(
			[this](const auto &solver) {
				_render.draw_system(solver);
			},
			_solver
		);
		_render.flush(ctx, q, uploader, swap_chain, size);
	}

	void soft_reset() override {
		_bodies.clear();
		_world = lotus::physics::world();
		std::visit(
			[this]<typename Solver>(Solver &solver) {
				solver = Solver();
				solver.physics_world = &_world;
			},
			_solver
		);

		_render = debug_render();
		_render.ctx = &_get_test_context();

		{
			auto [bullet_poly, bullet_poly_props] = create_box_shape(vec3::filled(_bullet_size));
			_bullet_shape = lotus::collision::shape::create(std::move(bullet_poly));
			_bullet_properties = bullet_poly_props.get_body_properties(_bullet_density);
		}
	}

	void gui() override {
		if (ImGui::BeginCombo("Solver", solver_creators[_solver.index()].name)) {
			for (usize i = 0; i < solver_creators.size(); ++i) {
				const solver_creator &creator = solver_creators[i];
				if (ImGui::Selectable(creator.name, i == _solver.index())) {
					creator.creator(_solver);
					soft_reset();
				}
			}
			ImGui::EndCombo();
		}
		std::visit(
			[this](auto &solver) {
				_solver_gui(solver);
			},
			_solver
		);

		ImGui::Separator();
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
	solver_variant _solver;
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

	void _solver_gui(lotus::physics::solvers::sequential_impulse::solver &solver) {
		ImGui_SliderT<u32>("Num Iterations", &solver.num_iterations, 1, 100);
	}
	void _solver_gui(lotus::physics::solvers::avbd::solver &solver) {
		ImGui_SliderT<u32>("Num Iterations", &solver.num_iterations, 1, 100);
		if (solver.has_indefinite_hessians) {
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Indefinite Hessians");
			solver.has_indefinite_hessians = false;
		}
	}
	void _solver_gui(lotus::physics::solvers::xpbd::solver &solver) {
		ImGui_SliderT<u32>("Num Iterations", &solver.num_iterations, 1, 100);
	}
};
