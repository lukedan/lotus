#include "lotus/physics/xpbd/solver.h"

/// \file
/// Implementation of the XPBD solver.

#include "lotus/physics/world.h"

namespace lotus::physics::xpbd {
	void solver::timestep(scalar dt, u32 iters) {
		scalar dt2 = dt * dt;
		scalar inv_dt2 = 1.0f / dt2;

		for (particle &p : particles) {
			p.prev_position = p.state.position;
			if (p.properties.inverse_mass > 0.0f) {
				p.state.velocity += dt * physics_world->gravity;
			}
			p.state.position += dt * p.state.velocity;
		}
		for (orientation &o : orientations) {
			o.prev_orientation = o.state.orientation;
			if (o.inv_inertia > 0.0f) {
				o.state.orientation = quatu::normalize(
					o.state.orientation +
					0.5f * dt * quat::from_vec3_xyz(o.state.angular_velocity) * o.state.orientation
				);
			}
		}
		for (body *b : physics_world->get_bodies()) {
			b->prev_position = b->state.position;
			// TODO external torque
			b->velocity_integration(dt, physics_world->gravity, zero);
			b->position_integration(dt);
		}

		{ // detect collisions
			contact_constraints.clear();
			const std::vector<world::collision_info> collisions = physics_world->detect_collisions();
			for (const world::collision_info &info : collisions) {
				contact_constraints.emplace_back(constraints::body_contact::create_for(
					*info.body1, *info.body2, info.contact.contact1, info.contact.contact2, info.contact.normal
				));
			}
		}

		// solve constraints
		contact_lambdas.resize(contact_constraints.size());
		std::ranges::fill(contact_lambdas, std::make_pair(0.0f, 0.0f));

		spring_lambdas.resize(particle_spring_constraints.size());
		std::ranges::fill(spring_lambdas, 0.0f);

		face_lambdas.resize(face_constraints.size(), uninitialized);
		std::ranges::fill(face_lambdas, zero);

		bend_lambdas.resize(bend_constraints.size());
		std::ranges::fill(bend_lambdas, 0.0f);

		rod_stretch_shear_lagrangians.resize(rod_stretch_shear_constraints.size(), zero);
		std::ranges::fill(rod_stretch_shear_lagrangians, zero);

		rod_bend_twist_lagrangians.resize(rod_bend_twist_constraints.size(), zero);
		std::ranges::fill(rod_bend_twist_lagrangians, zero);

		for (usize i = 0; i < iters; ++i) {
			// project body contact constraints
			for (usize j = 0; j < contact_constraints.size(); ++j) {
				contact_constraints[j].project(contact_lambdas[j].first, contact_lambdas[j].second);
			}

			// handle body-particle collisions
			for (const body *b : physics_world->get_bodies()) {
				if (b->properties.inverse_mass == 0.0f) {
					for (particle &p : particles) {
						std::visit(
							[&](const auto &shape) {
								handle_shape_particle_collision(shape, b->state, p.state.position);
							},
							b->body_shape->value
						);
					}
				}
			}

			// project spring constraints
			for (usize j = 0; j < particle_spring_constraints.size(); ++j) {
				const auto &s = particle_spring_constraints[j];
				particle &p1 = particles[s.particle1];
				particle &p2 = particles[s.particle2];
				s.project(
					p1.state.position, p2.state.position,
					p1.properties.inverse_mass, p2.properties.inverse_mass,
					inv_dt2, spring_lambdas[j]
				);
			}

			// project face constraints
			for (usize j = 0; j < face_constraints.size(); ++j) {
				constraints::face &f = face_constraints[j];
				particle &p1 = particles[f.particle1];
				particle &p2 = particles[f.particle2];
				particle &p3 = particles[f.particle3];
				f.project(
					p1.state.position, p2.state.position, p3.state.position,
					p1.properties.inverse_mass, p2.properties.inverse_mass, p3.properties.inverse_mass,
					inv_dt2, face_lambdas[j], face_constraint_projection_type
				);
			}

			// project bend constraints
			for (usize j = 0; j < bend_constraints.size(); ++j) {
				constraints::bend &b = bend_constraints[j];
				particle &p1 = particles[b.particle_edge1];
				particle &p2 = particles[b.particle_edge2];
				particle &p3 = particles[b.particle3];
				particle &p4 = particles[b.particle4];
				b.project(
					p1.state.position, p2.state.position, p3.state.position, p4.state.position,
					p1.properties.inverse_mass, p2.properties.inverse_mass,
					p3.properties.inverse_mass, p4.properties.inverse_mass,
					inv_dt2, bend_lambdas[j]
				);
			}

			// project rod bend-twist constraints
			for (usize j = 0; j < rod_bend_twist_constraints.size(); ++j) {
				const constraints::cosserat_rod::bend_twist &con = rod_bend_twist_constraints[j];
				con.project(
					orientations[con.orientation1],
					orientations[con.orientation2],
					inv_dt2,
					rod_bend_twist_lagrangians[j]
				);
			}

			// project rod stretch-shear constraints
			for (usize j = 0; j < rod_stretch_shear_constraints.size(); ++j) {
				const constraints::cosserat_rod::stretch_shear &con = rod_stretch_shear_constraints[j];
				con.project(
					particles[con.particle1],
					particles[con.particle2],
					orientations[con.orientation],
					inv_dt2,
					rod_stretch_shear_lagrangians[j]
				);
			}
		}

		for (particle &p : particles) {
			p.state.velocity = (p.state.position - p.prev_position) / dt;
		}
		for (orientation &o : orientations) {
			o.state.angular_velocity = (2.0f / dt) * (o.state.orientation * o.prev_orientation.conjugate()).axis();
		}
		for (body *b : physics_world->get_bodies()) {
			b->prev_velocity = b->state.velocity;

			b->state.velocity.linear = (b->state.position.position - b->prev_position.position) / dt;
			uquats dq = b->state.position.orientation * b->prev_position.orientation.inverse();
			b->state.velocity.angular = dq.axis() * (2.0f / dt);
			if (dq.w() < 0.0f) {
				b->state.velocity.angular = -b->state.velocity.angular;
			}
		}

		// velocity solve
		for (usize i = 0; i < contact_constraints.size(); ++i) {
			const auto &contact = contact_constraints[i];
			auto &b1 = *contact.body1;
			auto &b2 = *contact.body2;
			vec3 world_off1 = b1.state.position.orientation.rotate(contact.offset1);
			vec3 world_off2 = b2.state.position.orientation.rotate(contact.offset2);
			vec3 vel1 = b1.state.velocity.linear + vec::cross(b1.state.velocity.angular, world_off1);
			vec3 vel2 = b2.state.velocity.linear + vec::cross(b2.state.velocity.angular, world_off2);
			vec3 vel = vel1 - vel2;
			scalar vn = vec::dot(contact.normal, vel);
			vec3 vt = vel - contact.normal * vn;

			vec3 old_vel1 = b1.prev_velocity.linear + vec::cross(b1.prev_velocity.angular, world_off1);
			vec3 old_vel2 = b2.prev_velocity.linear + vec::cross(b2.prev_velocity.angular, world_off2);
			scalar old_vn = vec::dot(contact.normal, old_vel1 - old_vel2);

			scalar friction_coeff = std::min(b1.material.dynamic_friction, b2.material.dynamic_friction);
			scalar restitution_coeff = std::max(b1.material.restitution, b2.material.restitution);

			scalar vt_norm = vt.norm();
			scalar lambda_n = contact_lambdas[i].first;

			/*vec3 delta_vt_dir = -vt / vt_norm;
			scalar delta_vt_norm = std::min(friction_coeff * -lambda_n / dt, vt_norm);
			body::correction::compute(
				b1, b2, contact.offset1, contact.offset2, delta_vt_dir, 1.0
			).apply_velocity(delta_vt_norm);

			scalar delta_vn_norm = std::min(-old_vn * restitution_coeff, 0.0) - vn;
			body::correction::compute(
				b1, b2, contact.offset1, contact.offset2, contact.normal, 1.0
			).apply_velocity(delta_vn_norm);*/

			vec3 delta_vt = -vt * (std::min(friction_coeff * -lambda_n / dt, vt_norm) / vt_norm);
			vec3 delta_vn = contact.normal * (std::min<scalar>(-old_vn * restitution_coeff, 0.0f) - vn);
			vec3 delta_v = delta_vt + delta_vn;
			scalar delta_v_norm = delta_v.norm();
			vec3 delta_v_unit = delta_v / delta_v_norm;
			auto correction = constraints::body_contact::correction::compute(
				b1, b2, contact.offset1, contact.offset2, delta_v_unit, 1.0f
			);
			correction.apply_velocity(delta_v_norm);
		}
	}

	bool solver::handle_shape_particle_collision(
		const collision::shapes::plane&, const body_state &state, vec3 &pos
	) {
		vec3 plane_pos = state.position.orientation.inverse().rotate(pos - state.position.position);
		if (plane_pos[2] < 0.0f) {
			plane_pos[2] = 0.0f;
			pos = state.position.local_to_global(plane_pos);
			return true;
		}
		return false;
	}

	bool solver::handle_shape_particle_collision(
		const collision::shapes::sphere &shape, const body_state &state, vec3 &pos
	) {
		const vec3 diff = pos - state.position.local_to_global(shape.offset);
		scalar sqr_dist = diff.squared_norm();
		if (sqr_dist < shape.radius * shape.radius) {
			pos = state.position.position + diff * (shape.radius / std::sqrt(sqr_dist));
			return true;
		}
		return false;
	}

	bool solver::handle_shape_particle_collision(
		const collision::shapes::convex_polyhedron&, const body_state&, vec3&
	) {
		// TODO
		return false;
	}
}
