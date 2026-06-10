#include "lotus/physics/avbd/solver.h"

/// \file
/// Implementation of the AVBD solver.

#include "lotus/physics/world.h"

namespace lotus::physics::avbd {
	void solver::timestep(scalar dt, u32 iters) {
		// prepare rigid bodies
		_update_body_contacts();
		const _body_step_data body_step_data = _prepare_bodies(dt);
		_init_solve_bodies(dt, body_step_data);

		// prepare particles
		particle_prev_accelerations.resize(particles.size(), zero);
		const _particle_step_data particle_step_data = _prepare_particles(dt);
		_init_solve_particles(dt);

		// prepare orientations
		const _orientation_step_data orientation_step_data = _prepare_orientations();

		// iterations
		for (u32 iter = 0; iter < iters; ++iter) {
			_solve_bodies(dt, body_step_data);
			_solve_particles(dt, particle_step_data);
			_solve_orientations(orientation_step_data);
		}

		// naive collision handling
		for (particle &p : particles) {
			for (const body *b : physics_world->get_bodies()) {
				if (std::holds_alternative<collision::shapes::sphere>(b->body_shape->value)) {
					const auto &sphere = std::get<collision::shapes::sphere>(b->body_shape->value);
					const vec3 diff = p.state.position - b->state.position.position;
					const scalar diff_len = diff.norm();
					if (diff_len < sphere.radius) {
						p.state.position = b->state.position.position + diff * (sphere.radius / diff_len);
					}
				}
			}
		}

		_compute_body_velocities(dt, body_step_data);
		_compute_particle_velocities(dt, particle_step_data);
	}

	// rigid bodies
	void solver::_update_body_contacts() {
		// TODO: warm starting etc.
		std::vector<world::rigid_body_collision> new_contacts = physics_world->detect_collisions();
		contacts.clear();
		for (world::rigid_body_collision &c : new_contacts) {
			constraints::rigid_body_contact new_contact;
			new_contact.body1          = c.body1;
			new_contact.body2          = c.body2;
			new_contact.tangents       = tangent_frame<scalar>::from_normal(c.contact_manifold.normal);
			new_contact.contact_points = std::move(c.contact_manifold.points);
			new_contact.stiffness      = vec3::filled(1.0f);
			new_contact.lambda         = zero;
			contacts.emplace_back(new_contact);
		}
	}

	solver::_body_step_data solver::_prepare_bodies(scalar dt) const {
		_body_step_data result;

		const std::span<const body *const> bodies = physics_world->get_bodies();

		// compute initial and inertial positions
		result.initial_positions.reserve(bodies.size());
		result.inertial_positions.reserve(bodies.size());
		for (const body *b : bodies) {
			// initial position
			result.initial_positions.emplace_back(b->state.position);

			// compute inertial position
			body_position &inertial_pos = result.inertial_positions.emplace_back(uninitialized);
			inertial_pos.position =
				b->state.position.position + dt * (b->state.velocity.linear + dt * physics_world->gravity);
			inertial_pos.orientation = quatu::normalize(
				b->state.position.orientation +
				0.5f * quat::from_vec3_xyz(dt * b->state.velocity.angular) * b->state.position.orientation
			);
		}

		// compute per-constraint data
		result.constraint_association.resize(bodies.size(), {});
		for (usize i = 0; i < contacts.size(); ++i) {
			const constraints::rigid_body_contact &contact = contacts[i];

			// compute inverse constraint association
			auto find_idx = [&](const body *b) -> usize {
				for (usize ib = 0; ib < bodies.size(); ++ib) {
					if (bodies[ib] == b) {
						return ib;
					}
				}
				return 0;
			};
			result.constraint_association[find_idx(contact.body1)].contact_constraints.emplace_back(i);
			result.constraint_association[find_idx(contact.body2)].contact_constraints.emplace_back(i);
		}

		return result;
	}

	void solver::_init_solve_bodies(scalar dt, const _body_step_data &bdata) {
		const std::span<body *const> bodies = physics_world->get_bodies();
		for (usize i = 0; i < bodies.size(); ++i) {
			body *cur_body = bodies[i];
			if (cur_body->properties.inverse_mass <= 0.0f) {
				continue;
			}

			const body_position inertial_pos = bdata.inertial_positions[i];
			//cur_body->state.position = inertial_pos;
		}
	}

	void solver::_solve_bodies(scalar dt, const _body_step_data &bdata) {
		using _vec6 = column_vector<6, scalar>;
		using _mat66 = matrix<6, 6, scalar>;

		const std::span<body *const> bodies = physics_world->get_bodies();
		for (usize index = 0; index < bodies.size(); ++index) {
			body *cur_body = bodies[index];
			body_position &cur_pos = cur_body->state.position;
			if (cur_body->properties.inverse_mass <= 0.0f) {
				continue;
			}

			const body_position inertial_pos = bdata.inertial_positions[index];
			const mat33s local_to_world = cur_pos.orientation.into_rotation_matrix();
			const mat33s inertia_ws = local_to_world.transposed() * cur_body->properties.inertia * local_to_world;

			// inertial terms for f and h
			_vec6 f = (-1.0f / (dt * dt)) * _vec6(
				(cur_pos.position - inertial_pos.position) / cur_body->properties.inverse_mass,
				inertia_ws * 2.0f * (cur_pos.orientation * inertial_pos.orientation.conjugate()).axis()
			);
			_mat66 h = (1.0f / (dt * dt)) * mat::concat_rows(
				mat::concat_columns((1.0f / cur_body->properties.inverse_mass) * mat33s::identity(), mat33s(zero)),
				mat::concat_columns(mat33s(zero), inertia_ws)
			);

			// constraint terms for f and h
			for (const u32 ci : bdata.constraint_association[index].contact_constraints) {
				const constraints::rigid_body_contact &contact = contacts[ci];
				const scalar sign = cur_body == contact.body1 ? 1.0f : -1.0f;
				for (const collision::contact_manifold::point &contact_point : contact.contact_points) {
					const vec3 r_local =
						cur_body == contact.body1 ? contact_point.local_position1 : contact_point.local_position2;
					const vec3 r = cur_pos.orientation.rotate(r_local);

					const vec3 penetration =
						contact.body1->state.position.local_to_global(contact_point.local_position1) -
						contact.body2->state.position.local_to_global(contact_point.local_position2);
					const scalar c = vec::dot(penetration, contact.tangents.normal);
					if (c < 0.0f) {
						continue;
					}

					const scalar stiffness = 1000.0f;
					const _vec6 dcdx = sign * _vec6(contact.tangents.normal, vec::cross(r, contact.tangents.normal));

					// f term
					f -= stiffness * std::max<scalar>(0.0f, c) * dcdx;

					// h term
					h += stiffness * dcdx * dcdx.transposed();
				}
			}

			// solve
			const auto decomposition = lup_decomposition<6, scalar>::compute(h);
			const scalar det = decomposition.determinant();
			if (!std::isfinite(det) || std::abs(det) < 1e-6) {
				continue;
			}
			const _vec6 delta_x = decomposition.solve(f);

			// update
			cur_pos.position += delta_x.block<3, 1>(0, 0);
			cur_pos.orientation = quatu::normalize(
				cur_pos.orientation + 0.5f * quat::from_vec3_xyz(delta_x.block<3, 1>(3, 0)) * cur_pos.orientation
			);
		}
	}

	void solver::_compute_body_velocities(scalar dt, const _body_step_data &bdata) {
		const std::span<body *const> bodies = physics_world->get_bodies();
		for (usize i = 0; i < bodies.size(); ++i) {
			body *cur_body = bodies[i];
			if (cur_body->properties.inverse_mass <= 0.0f) {
				continue;
			}

			const body_position initial_pos = bdata.initial_positions[i];

			cur_body->state.velocity.linear = (cur_body->state.position.position - initial_pos.position) / dt;
			cur_body->state.velocity.angular =
				(2.0f / dt) * (cur_body->state.position.orientation * initial_pos.orientation.conjugate()).axis();
		}
	}

	// particles
	solver::_particle_step_data solver::_prepare_particles(scalar dt) const {
		_particle_step_data result;

		// compute initial and inertial positions
		result.initial_positions.reserve(particles.size());
		result.inertial_positions.reserve(particles.size());
		for (const particle &p : particles) {
			result.initial_positions.emplace_back(p.state.position);
			result.inertial_positions.emplace_back(
				p.state.position + dt * (p.state.velocity + dt * physics_world->gravity)
			);
		}

		// compute inverse constraint association
		result.constraint_association.resize(particles.size());
		for (usize i = 0; i < rod_stretch_shear_constraints.size(); ++i) {
			const constraints::cosserat_rod::stretch_shear &sc = rod_stretch_shear_constraints[i];
			result.constraint_association[sc.particle1].stretch_shear_constraints.emplace_back(static_cast<u32>(i));
			result.constraint_association[sc.particle2].stretch_shear_constraints.emplace_back(static_cast<u32>(i));
		}

		return result;
	}

	void solver::_init_solve_particles(scalar dt) {
		for (usize i = 0; i < particles.size(); ++i) {
			particle &p = particles[i];
			if (p.properties.inverse_mass <= 0.0f) {
				continue;
			}

			// compute adaptive acceleration
			const vec3 prev_accel = particle_prev_accelerations[i];
			const vec3 cur_accel = physics_world->gravity;
			const scalar len_factor = std::clamp<scalar>(
				vec::dot(prev_accel, cur_accel) / cur_accel.squared_norm(), 0.0f, 1.0f
			);
			const vec3 adaptive_accel = cur_accel * len_factor;
			p.state.position += dt * (p.state.velocity + dt * adaptive_accel);
		}
	}

	void solver::_solve_particles(scalar dt, const _particle_step_data &pdata) {
		for (usize index = 0; index < particles.size(); ++index) {
			particle &p = particles[index];
			if (p.properties.inverse_mass <= 0.0f) {
				continue;
			}

			vec3 f = (pdata.inertial_positions[index] - p.state.position) / (dt * dt * p.properties.inverse_mass);
			// the hessian for Cosserat rods simplifies to a multiple of the identity matrix
			scalar h = 1.0f / (dt * dt * p.properties.inverse_mass);
			for (const u32 ss_constraint_index : pdata.constraint_association[index].stretch_shear_constraints) {
				const constraints::cosserat_rod::stretch_shear &constraint =
					rod_stretch_shear_constraints[ss_constraint_index];
				const scalar stiffness = constraint.stiffness * constraint.initial_length;
				const particle &p1 = particles[constraint.particle1];
				const particle &p2 = particles[constraint.particle2];
				const orientation &o = orientations[constraint.orientation];
				const vec3 c =
					(p2.state.position - p1.state.position) / constraint.initial_length -
					o.state.orientation.rotate(constraints::cosserat_rod::direction_basis);
				const scalar derivative_sign = index == constraint.particle1 ? -1 : 1;
				f -= (derivative_sign * stiffness / constraint.initial_length) * c;
				h += stiffness / (constraint.initial_length * constraint.initial_length);
			}

			p.state.position += f / h;
		}
	}

	void solver::_compute_particle_velocities(scalar dt, const _particle_step_data &pdata) {
		for (usize index = 0; index < particles.size(); ++index) {
			particle &p = particles[index];
			const vec3 new_velocity = (p.state.position - pdata.initial_positions[index]) / dt;
			particle_prev_accelerations[index] = (new_velocity - p.state.velocity) / dt;
			p.state.velocity = new_velocity;
		}
	}

	// orientation
	solver::_orientation_step_data solver::_prepare_orientations() const {
		_orientation_step_data result;

		result.constraint_association.resize(orientations.size());
		for (usize i = 0; i < rod_bend_twist_constraints.size(); ++i) {
			const constraints::cosserat_rod::bend_twist &bc = rod_bend_twist_constraints[i];
			result.constraint_association[bc.orientation1].bend_constraints.emplace_back(static_cast<u32>(i));
			result.constraint_association[bc.orientation2].bend_constraints.emplace_back(static_cast<u32>(i));
		}
		for (usize i = 0; i < rod_stretch_shear_constraints.size(); ++i) {
			const constraints::cosserat_rod::stretch_shear &sc = rod_stretch_shear_constraints[i];
			crash_if(
				result.constraint_association[sc.orientation].stretch_shear_constraint !=
				std::numeric_limits<u32>::max()
			);
			result.constraint_association[sc.orientation].stretch_shear_constraint = static_cast<u32>(i);
		}

		return result;
	}

	void solver::_solve_orientations(const _orientation_step_data &odata) {
		for (usize index = 0; index < orientations.size(); ++index) {
			orientation &ori = orientations[index];
			if (ori.inv_inertia <= 0.0f) {
				continue;
			}

			const _orientation_step_data::constraints &assoc = odata.constraint_association[index];
			crash_if(assoc.stretch_shear_constraint == std::numeric_limits<u32>::max());

			// stretching shearing part
			const constraints::cosserat_rod::stretch_shear &sc =
				rod_stretch_shear_constraints[assoc.stretch_shear_constraint];
			const vec3 v =
				-2.0f * sc.stiffness *
				(particles[sc.particle2].state.position - particles[sc.particle1].state.position);

			// bending part
			quats b = zero;
			for (const u32 bend_id : assoc.bend_constraints) {
				const constraints::cosserat_rod::bend_twist &bc = rod_bend_twist_constraints[bend_id];
				b += bc.stiffness * (
					index == bc.orientation1 ?
					orientations[bc.orientation2].state.orientation * bc.initial_bend :
					orientations[bc.orientation1].state.orientation * bc.initial_bend.conjugate()
				);
			}

			const scalar v2 = v.squared_norm();

			const scalar lambda = std::sqrt(v2) + b.magnitude();
			const quats qv = quat::from_vec3_xyz(v);
			const quats qe = quat::from_vec3_xyz(constraints::cosserat_rod::direction_basis);
			ori.state.orientation = quatu::normalize(qv * b * qe + lambda * b);
		}
	}
}
