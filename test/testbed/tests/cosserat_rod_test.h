#pragma once

#include "lotus/physics/avbd/solver.h"

#include "test.h"

class cosserat_rod_test : public test {
public:
	explicit cosserat_rod_test(const test_context &tctx) : test(tctx) {
		soft_reset();
	}

	void timestep(scalar dt, u32 iterations) override {
		// move the rods around
		const auto pos_at = [](scalar t) {
			return 0.2f * vec3(std::sin(t), std::sin(1.3f * t), 0.0f);
		};
		const vec3 offset = pos_at(_time + dt * _move_scale) - pos_at(_time);
		for (lotus::physics::particle &p : _solver_avbd.particles) {
			if (p.properties.inverse_mass > 0.0f) {
				continue;
			}
			p.state.position += offset;
		}
		for (lotus::physics::particle &p : _solver_xpbd.particles) {
			if (p.properties.inverse_mass > 0.0f) {
				continue;
			}
			p.state.position += offset;
		}

		// move the collider around
		const auto collider_pos_at = [](scalar t) {
			return 0.05f * vec3(std::sin(t), std::sin(1.7f * t), 0.0f);
		};
		const vec3 collider_offset = collider_pos_at(_collider_time + dt * _collider_move_scale) - collider_pos_at(_collider_time);
		_sphere_avbd.state.position.position += collider_offset;
		_sphere_xpbd.state.position.position += collider_offset;


		_solver_avbd.timestep(dt, iterations);
		_solver_xpbd.timestep(dt, iterations);

		_time += dt * _move_scale;
		_collider_time += dt * _collider_move_scale;
	}

	void render(
		lotus::renderer::context &ctx,
		lotus::renderer::context::queue &q,
		lotus::renderer::constant_uploader &uploader,
		lotus::renderer::image2d_color color,
		lotus::renderer::image2d_depth_stencil depth,
		lotus::cvec2u32 size
	) override {
		_render.draw_system(_solver_avbd);
		_render.draw_system(_solver_xpbd);
		_render.flush(ctx, q, uploader, color, depth, size);
	}

	void soft_reset() override {
		const lotus::physics::material_properties phys_mat(1.0f, 1.0f, 0.0f);

		_sphere_shape.value.emplace<lotus::collision::shapes::sphere>(lotus::collision::shapes::sphere::from_radius(0.03f));
		_sphere_avbd = lotus::physics::body::create(
			_sphere_shape,
			phys_mat,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(_pos_avbd + vec3(0.0f, 0.0f, 0.5f * _length_m), uquats::identity())
		);
		_sphere_xpbd = lotus::physics::body::create(
			_sphere_shape,
			phys_mat,
			lotus::physics::body_properties::kinematic(),
			lotus::physics::body_state::stationary_at(_pos_xpbd + vec3(0.0f, 0.0f, 0.5f * _length_m), uquats::identity())
		);

		_world_avbd = lotus::physics::world();
		_world_avbd.gravity = vec3(0.0f, -9.8f, 0.0f);
		_world_avbd.add_body(_sphere_avbd);
		_solver_avbd = lotus::physics::avbd::solver();
		_solver_avbd.physics_world = &_world_avbd;

		_world_xpbd = lotus::physics::world();
		_world_xpbd.gravity = vec3(0.0f, -9.8f, 0.0f);
		_world_xpbd.add_body(_sphere_xpbd);
		_solver_xpbd = lotus::physics::xpbd::solver();
		_solver_xpbd.physics_world = &_world_xpbd;

		_render = debug_render();
		_render.ctx = &_get_test_context();

		_time = 0.0f;
		_collider_time = 0.0f;

		for (u32 x = 0; x < 5; ++x) {
			for (u32 y = 0; y < 5; ++y) {
				const vec3 start(0.01f * x, 0.01f * y, 0.0f);
				const vec3 end = start + vec3(0.0f, 0.0f, _length_m);

				_create_straight_rod_avbd(
					start + _pos_avbd, end + _pos_avbd, _segments, _density_kg_m3, _diameter_m, _k_ss, _k_bt
				);

				_create_straight_rod_xpbd(
					start + _pos_xpbd, end + _pos_xpbd, _segments, _density_kg_m3, _diameter_m, _k_ss, _k_bt
				);
			}
		}
	}

	void gui() override {
		const u32 min_segments = 2;
		const u32 max_segments = 100;
		ImGui::SliderScalar("Num Segments", ImGuiDataType_U32, &_segments, &min_segments, &max_segments);
		ImGui::SliderFloat("Density kg/m3", &_density_kg_m3, 100.0f, 10000.0f);
		ImGui::SliderFloat("Length", &_length_m, 0.01f, 1.0f);
		ImGui::SliderFloat("Diameter m", &_diameter_m, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Stretching-Shearing Stiffness", &_k_ss, 0.0f, 10000.0f, "%.5f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Bending Stiffness", &_k_bt, 0.0f, 10000.0f, "%.5f", ImGuiSliderFlags_Logarithmic);

		ImGui::Separator();
		ImGui::SliderFloat("Move Time Scale", &_move_scale, 0.0f, 10.0f);
		ImGui::SliderFloat("Collider Move Time Scale", &_collider_move_scale, 0.0f, 10.0f);

		test::gui();
	}

	inline static std::string get_name() {
		return "Cosserat Rod";
	}
private:
	constexpr static vec3 _pos_avbd = vec3(0.0f, 0.0f, 0.0f);
	constexpr static vec3 _pos_xpbd = vec3(0.5f, 0.0f, 0.0f);

	lotus::collision::shape _sphere_shape;
	lotus::physics::body _sphere_avbd = lotus::uninitialized;
	lotus::physics::body _sphere_xpbd = lotus::uninitialized;

	lotus::physics::world _world_avbd;
	lotus::physics::avbd::solver _solver_avbd;
	lotus::physics::world _world_xpbd;
	lotus::physics::xpbd::solver _solver_xpbd;
	debug_render _render;
	scalar _time = 0.0f;
	scalar _collider_time = 0.0f;

	u32 _segments = 10;
	scalar _density_kg_m3 = 1000.0f;
	scalar _length_m = 0.2f; // 20cm
	scalar _diameter_m = 0.05f; // 5cm
	scalar _k_ss = 1.0f;
	scalar _k_bt = 1.0f;

	scalar _move_scale = 0.0f;
	scalar _collider_move_scale = 0.0f;

	template <typename Solver, typename BendCallback, typename StretchCallback> static void _create_straight_rod(
		Solver &solver, BendCallback &&bend_cb, StretchCallback &&stretch_cb,
		vec3 start, vec3 end, u32 num_parts, scalar density, scalar diameter
	) {
		const float volume = 0.25f * static_cast<f32>(lotus::constants::pi) * diameter * diameter * (end - start).norm();
		const float total_mass = volume * density;

		const vec3 part_offset = (end - start) / (num_parts - 1);
		const scalar inv_part_mass = static_cast<scalar>(num_parts) / total_mass;
		const scalar inv_inertia_mass = 8.0f * inv_part_mass / (diameter * diameter);

		// add particles
		const auto first_part = static_cast<u32>(solver.particles.size());
		for (u32 i = 0; i < num_parts; ++i) {
			lotus::physics::particle_properties props = lotus::uninitialized;
			props.inverse_mass = i < 2 ? 0.0f : inv_part_mass;
			solver.particles.emplace_back(lotus::physics::particle::create(
				props, lotus::physics::particle_state::stationary_at(start + part_offset * i)
			));
		}

		// add orientations
		const auto first_ori = static_cast<u32>(solver.orientations.size());
		const uquats all_ori = lotus::quat::from_normalized_from_to(
			lotus::physics::avbd::constraints::cosserat_rod::direction_basis,
			lotus::vecu::normalize(part_offset)
		);
		for (u32 i = 1; i < num_parts; ++i) {
			lotus::physics::orientation &ori = solver.orientations.emplace_back(lotus::uninitialized);
			ori.state            = lotus::physics::orientation_state::stationary_at(all_ori);
			ori.prev_orientation = ori.state.orientation;
			ori.inv_inertia      = i < 2 ? 0.0f : inv_inertia_mass;
		}

		// set up bending-twisting constraints
		for (u32 i = 2; i < num_parts; ++i) {
			const u32 ori1 = first_ori + i - 2;
			const u32 ori2 = first_ori + i - 1;
			const uquats init_bend =
				solver.orientations[ori1].state.orientation.conjugate() *
				solver.orientations[ori2].state.orientation;
			bend_cb(ori1, ori2, init_bend);
		}

		// set up stretching-shearing constraints
		// skip the first segment: all elements are kinematic
		for (u32 i = 2; i < num_parts; ++i) {
			stretch_cb(first_part + i - 1, first_part + i, first_ori + i - 1, part_offset.norm());
		}
	}

	void _create_straight_rod_xpbd(
		vec3 start, vec3 end, u32 num_parts, scalar density, scalar diameter, scalar k_ss, scalar k_bt
	) {
		_create_straight_rod(
			_solver_xpbd,
			[&](u32 o1, u32 o2, uquats initial_bend) {
				lotus::physics::xpbd::constraints::cosserat_rod::bend_twist &constraint =
					_solver_xpbd.rod_bend_twist_constraints.emplace_back(lotus::uninitialized);
				constraint.orientation1 = o1;
				constraint.orientation2 = o2;
				constraint.initial_bend = initial_bend;
				constraint.compliance   = 1.0f / k_bt;
			},
			[&](u32 p1, u32 p2, u32 o, scalar len) {
				lotus::physics::xpbd::constraints::cosserat_rod::stretch_shear &constraint =
					_solver_xpbd.rod_stretch_shear_constraints.emplace_back(lotus::uninitialized);
				constraint.particle1          = p1;
				constraint.particle2          = p2;
				constraint.orientation        = o;
				constraint.inv_initial_length = 1.0f / len;
				constraint.compliance         = 1.0f / k_ss;
			},
			start,
			end,
			num_parts,
			density,
			diameter
		);
	}
	void _create_straight_rod_avbd(
		vec3 start, vec3 end, u32 num_parts, scalar density, scalar diameter, scalar k_ss, scalar k_bt
	) {
		_create_straight_rod(
			_solver_avbd,
			[&](u32 o1, u32 o2, uquats init_bend) {
				lotus::physics::avbd::constraints::cosserat_rod::bend_twist &constraint =
					_solver_avbd.rod_bend_twist_constraints.emplace_back(lotus::uninitialized);
				constraint.orientation1 = o1;
				constraint.orientation2 = o2;
				constraint.initial_bend = init_bend;
				constraint.stiffness    = k_bt;
			},
			[&](u32 p1, u32 p2, u32 o, scalar len) {
				lotus::physics::avbd::constraints::cosserat_rod::stretch_shear &constraint =
					_solver_avbd.rod_stretch_shear_constraints.emplace_back(lotus::uninitialized);
				constraint.particle1      = p1;
				constraint.particle2      = p2;
				constraint.orientation    = o;
				constraint.initial_length = len;
				constraint.stiffness      = k_ss;
			},
			start,
			end,
			num_parts,
			density,
			diameter
		);
	}
};
