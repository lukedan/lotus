#include "lotus/physics/avbd/solver.h"

/// \file
/// Implementation of the AVBD solver.

#include <set>

#include "lotus/physics/world.h"

namespace lotus::physics::avbd {
	solver::_contact_force solver::_contact_force::clamp(
		vec3 force, const material_properties &mat1, const material_properties &mat2
	) {
		_contact_force result = zero;
		if (force[0] < 0.0f) {
			result.normal_clamped   = true;
			result.friction_clamped = true;
			return result;
		}
		result.force = force;
		// clamp friction force
		const scalar static_friction_force = std::min(mat1.static_friction, mat2.static_friction) * force[0];
		const scalar dynamic_friction_force = std::min(mat1.dynamic_friction, mat2.dynamic_friction) * force[0];
		const vec2 friction_force = force.block<2, 1>(1, 0);
		if (friction_force.squared_norm() >= static_friction_force * static_friction_force) {
			const scalar scale = dynamic_friction_force / std::max<scalar>(1e-6f, friction_force.norm());
			result.force = vec3(force[0], friction_force * scale);
			result.friction_clamped = true;
		}
		return result;
	}


	void solver::timestep(scalar dt, u32 iters) {
		// prepare rigid bodies
		_update_body_contacts();
		_body_step_data body_step_data = _prepare_bodies(dt);
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

			_update_body_dual_variables(body_step_data);
		}

		_compute_body_velocities(dt, body_step_data);
		_compute_particle_velocities(dt, particle_step_data);
	}

	// rigid bodies
	vec3 solver::_compute_raw_contact_error(
		const constraints::rigid_body_contact &contact,
		const constraints::rigid_body_contact::point &contact_point
	) const {
		const vec3 penetration =
			contact.body1->state.position.local_to_global(contact_point.local_position1) -
			contact.body2->state.position.local_to_global(contact_point.local_position2);
		return {
			vec::dot(penetration, contact.tangents.normal) - physics_world->collision_threshold,
			vec::dot(penetration, contact.tangents.tangent),
			vec::dot(penetration, contact.tangents.bitangent)
		};
	}

	void solver::_update_body_contacts() {
		// collect pairs of bodies that have collision disabled explicitly
		std::set<std::pair<body*, body*>> collision_disabled;
		for (const constraints::spring &spring : springs) {
			if (spring.disable_collision && spring.body1 && spring.body2) {
				collision_disabled.emplace(spring.body1, spring.body2);
				collision_disabled.emplace(spring.body2, spring.body1);
			}
		}
		for (const constraints::pin &pin : pins) {
			if (pin.disable_collision && pin.body1 && pin.body2) {
				collision_disabled.emplace(pin.body1, pin.body2);
				collision_disabled.emplace(pin.body2, pin.body1);
			}
		}
		for (const constraints::hinge &hinge : hinges) {
			if (hinge.disable_collision && hinge.body1 && hinge.body2) {
				collision_disabled.emplace(hinge.body1, hinge.body2);
				collision_disabled.emplace(hinge.body2, hinge.body1);
			}
		}

		std::vector<world::rigid_body_collision> new_contacts = physics_world->detect_collisions();
		contacts.clear();
		for (const world::rigid_body_collision &c : new_contacts) {
			if (collision_disabled.find(std::make_pair(c.body1, c.body2)) != collision_disabled.end()) {
				continue;
			}
			constraints::rigid_body_contact &new_contact = contacts.emplace_back();
			new_contact.body1    = c.body1;
			new_contact.body2    = c.body2;
			new_contact.tangents = tangent_frame<scalar>::from_normal(c.contact_manifold.normal);
			for (const collision::contact_manifold::point &p : c.contact_manifold.points) {
				constraints::rigid_body_contact::point &point = new_contact.contact_points.emplace_back();
				point.local_position1 = p.local_position1;
				point.local_position2 = p.local_position2;
			}
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

		// compute inverse constraint association
		const auto find_body_idx = [&](const body *b) -> usize {
			for (usize ib = 0; ib < bodies.size(); ++ib) {
				if (bodies[ib] == b) {
					return ib;
				}
			}
			std::abort();
		};
		result.constraint_association.resize(bodies.size(), {});
		for (usize i = 0; i < contacts.size(); ++i) {
			const constraints::rigid_body_contact &contact = contacts[i];
			result.constraint_association[find_body_idx(contact.body1)].contact_constraints.emplace_back(i);
			result.constraint_association[find_body_idx(contact.body2)].contact_constraints.emplace_back(i);
		}
		for (usize i = 0; i < springs.size(); ++i) {
			const constraints::spring &spring = springs[i];
			if (spring.body1) {
				result.constraint_association[find_body_idx(spring.body1)].spring_constraints.emplace_back(i);
			}
			if (spring.body2) {
				result.constraint_association[find_body_idx(spring.body2)].spring_constraints.emplace_back(i);
			}
		}
		for (usize i = 0; i < pins.size(); ++i) {
			const constraints::pin &pin = pins[i];
			if (pin.body1) {
				result.constraint_association[find_body_idx(pin.body1)].pin_constraints.emplace_back(i);
			}
			if (pin.body2) {
				result.constraint_association[find_body_idx(pin.body2)].pin_constraints.emplace_back(i);
			}
		}
		for (usize i = 0; i < hinges.size(); ++i) {
			const constraints::hinge &hinge = hinges[i];
			if (hinge.body1) {
				result.constraint_association[find_body_idx(hinge.body1)].hinge_constraints.emplace_back(i);
			}
			if (hinge.body2) {
				result.constraint_association[find_body_idx(hinge.body2)].hinge_constraints.emplace_back(i);
			}
		}

		// initialize contact dual variables
		result.contact_duals.reserve(contacts.size());
		for (const constraints::rigid_body_contact &contact : contacts) {
			_contact_dual &dual = result.contact_duals.emplace_back();
			dual.contact_points.reserve(contact.contact_points.size());
			for (const constraints::rigid_body_contact::point &contact_point : contact.contact_points) {
				_contact_dual::point &point = dual.contact_points.emplace_back(zero);
				// TODO warm starting
				point.stiffness     = vec3::filled(1000.0f);
				point.force         = zero;
				point.initial_error = _compute_raw_contact_error(contact, contact_point);
			}
		}

		// initialize pin dual variables
		result.pin_duals.reserve(pins.size());
		for (usize i = 0; i < pins.size(); ++i) {
			_pin_dual &dual = result.pin_duals.emplace_back(zero);
			// TODO warm starting
			dual.stiffness = vec3::filled(1000.0f);
			dual.force     = zero;
		}

		// initialize hinge dual variables
		result.hinge_duals.reserve(hinges.size());
		for (usize i = 0; i < hinges.size(); ++i) {
			_hinge_dual &dual = result.hinge_duals.emplace_back(zero);
			// TODO warm starting
			dual.stiffness = 1000.0f;
			dual.force     = 0.0f;
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
		for (usize bi = 0; bi < bodies.size(); ++bi) {
			body *cur_body = bodies[bi];
			crash_if(cur_body->state.position.position.has_nan());
			body_position &cur_pos = cur_body->state.position;
			if (cur_body->properties.inverse_mass <= 0.0f) {
				continue;
			}

			const body_position inertial_pos = bdata.inertial_positions[bi];
			const _body_step_data::constraints &constraint_association = bdata.constraint_association[bi];

			const mat33s local_to_world = cur_pos.orientation.into_rotation_matrix();
			const mat33s inertia_ws = local_to_world.transposed() * cur_body->properties.inertia * local_to_world;

			// inertial terms
			_vec6 f = (-1.0f / (dt * dt)) * _vec6(
				(cur_pos.position - inertial_pos.position) / cur_body->properties.inverse_mass,
				inertia_ws * 2.0f * (cur_pos.orientation * inertial_pos.orientation.conjugate()).axis()
			);
			_mat66 h = (1.0f / (dt * dt)) * mat::concat_rows(
				mat::concat_columns((1.0f / cur_body->properties.inverse_mass) * mat33s::identity(), mat33s(zero)),
				mat::concat_columns(mat33s(zero), inertia_ws)
			);

			// contact terms
			for (const u32 ci : constraint_association.contact_constraints) {
				const constraints::rigid_body_contact &contact = contacts[ci];
				const _contact_dual &dual = bdata.contact_duals[ci];
				const scalar sign = cur_body == contact.body1 ? 1.0f : -1.0f;
				for (usize cpi = 0; cpi < contact.contact_points.size(); ++cpi) {
					const constraints::rigid_body_contact::point &contact_point = contact.contact_points[cpi];
					const _contact_dual::point &dual_point = dual.contact_points[cpi];

					const vec3 r_local =
						cur_body == contact.body1 ? contact_point.local_position1 : contact_point.local_position2;
					const vec3 r = cur_pos.orientation.rotate(r_local);

					// compute constraint terms
					const vec3 c = _compute_contact_error(contact, contact_point, dual_point);
					const vec3 unclamped_force = matm::multiply(dual_point.stiffness, c) + dual_point.force;
					const auto force =
						_contact_force::clamp(unclamped_force, contact.body1->material, contact.body2->material);

					const _vec6 dcndx =
						sign * _vec6(contact.tangents.normal, vec::cross(r, contact.tangents.normal));
					const _vec6 dctdx =
						sign * _vec6(contact.tangents.tangent, vec::cross(r, contact.tangents.tangent));
					const _vec6 dcbdx =
						sign * _vec6(contact.tangents.bitangent, vec::cross(r, contact.tangents.bitangent));

					const mat33s d2cndq2 =
						sign * vec::cross_matrix(r) * vec::cross_matrix(contact.tangents.normal);
					const mat33s d2ctdq2 =
						sign * vec::cross_matrix(r) * vec::cross_matrix(contact.tangents.tangent);
					const mat33s d2cbdq2 =
						sign * vec::cross_matrix(r) * vec::cross_matrix(contact.tangents.bitangent);

					f -= mat::concat_columns(dcndx, dctdx, dcbdx) * force.force;

					h +=
						dual_point.stiffness[0] * (dcndx * dcndx.transposed()) +
						dual_point.stiffness[1] * (dctdx * dctdx.transposed()) +
						dual_point.stiffness[2] * (dcbdx * dcbdx.transposed());

					mat33s hrot = h.block<3, 3>(3, 3);
					hrot +=
						dual_point.force[0] * d2cndq2 +
						dual_point.force[1] * d2ctdq2 +
						dual_point.force[2] * d2cbdq2;
					h.set_block(3, 3, hrot);
				}
			}

			// spring terms
			for (const u32 ci : constraint_association.spring_constraints) {
				const constraints::spring &spring = springs[ci];

				const vec3 r1 =
					spring.body1 ?
					spring.body1->state.position.orientation.rotate(spring.local_position1) :
					spring.local_position1;
				const vec3 r2 =
					spring.body2 ?
					spring.body2->state.position.orientation.rotate(spring.local_position2) :
					spring.local_position2;
				const vec3 d = spring.get_global_position1() - spring.get_global_position2();
				const scalar d_len = d.norm();
				const vec3 d_norm = d / d_len;
				const scalar c = d_len - spring.initial_length;
				const scalar stiffness = c > 0.0f ? spring.stretched_stiffness : spring.compressed_stiffness;

				const scalar sign = cur_body == spring.body1 ? 1.0f : -1.0f;
				const vec3 r = cur_body == spring.body1 ? r1 : r2;

				const _vec6 dcdx = sign * _vec6(d_norm, vec::cross(r, d_norm));
				const _mat66 ddt = dcdx * dcdx.transposed();

				const mat33s rx = vec::cross_matrix(r);
				_mat66 b = _mat66::identity();
				b.set_block(3, 0, rx);
				b.set_block(0, 3, -rx);
				b.set_block(3, 3, sign * rx * vec::cross_matrix(d - sign * r));

				f -= stiffness * c * dcdx;

				/*
				const _mat66 d2cdx2 = b / d_len - ddt / (d_len * d_len * d_len);
				h += stiffness * (ddt + c * d2cdx2);
				*/
				// offers better numerical stability for stiff springs
				// the cubic term and the constraint term can get very small
				h += stiffness * ddt + stiffness * (1.0f - spring.initial_length / d_len) * b;
			}

			// pin terms
			for (const u32 ci : constraint_association.pin_constraints) {
				const constraints::pin &pin = pins[ci];
				const _pin_dual &dual = bdata.pin_duals[ci];

				const scalar sign = cur_body == pin.body1 ? 1.0 : -1.0f;
				const vec3 r_local = cur_body == pin.body1 ? pin.local_position1 : pin.local_position2;
				const vec3 r = cur_body->state.position.orientation.rotate(r_local);
				const vec3 c = pin.get_global_position1() - pin.get_global_position2();
				const vec3 force = matm::multiply(dual.stiffness, c) + dual.force;

				matrix<6, 3, scalar> dcdx = uninitialized;
				dcdx.set_block(0, 0, sign * mat33s::identity());
				dcdx.set_block(3, 0, sign * vec::cross_matrix(r));

				const mat33s d2cdwx2 = vec::cross_matrix(r) * vec::cross_matrix(vec3(1.0f, 0.0f, 0.0f));
				const mat33s d2cdwy2 = vec::cross_matrix(r) * vec::cross_matrix(vec3(0.0f, 1.0f, 0.0f));
				const mat33s d2cdwz2 = vec::cross_matrix(r) * vec::cross_matrix(vec3(0.0f, 0.0f, 1.0f));

				f -= dcdx * force;

				h +=
					dual.stiffness[0] * dcdx.column(0) * dcdx.column(0).transposed() +
					dual.stiffness[1] * dcdx.column(1) * dcdx.column(1).transposed() +
					dual.stiffness[2] * dcdx.column(2) * dcdx.column(2).transposed();

				mat33s hrot = h.block<3, 3>(3, 3);
				hrot += force[0] * d2cdwx2 + force[1] * d2cdwy2 + force[2] * d2cdwz2;
				h.set_block(3, 3, hrot);
			}

			// hinge terms
			for (const u32 ci : constraint_association.hinge_constraints) {
				const constraints::hinge &hinge = hinges[ci];
				const _hinge_dual &dual = bdata.hinge_duals[ci];

				vec3 r1 = hinge.get_global_axis1();
				vec3 r2 = hinge.get_global_axis2();
				if (cur_body == hinge.body2) {
					std::swap(r1, r2);
				}

				const scalar c = hinge_constant - vec::dot(r1, r2);
				const scalar force = dual.stiffness * c + dual.force;

				const vec3 dcdw = vec::cross(r2, r1);
				const mat33 d2cdw2 = -vec::cross_matrix(r1) * vec::cross_matrix(r2);

				vec3 frot = f.block<3, 1>(3, 0);
				frot -= force * dcdw;
				f.set_block(3, 0, frot);

				mat33s hrot = h.block<3, 3>(3, 3);
				hrot += dual.stiffness * dcdw * dcdw.transposed() + force * d2cdw2;
				h.set_block(3, 3, hrot);
			}

			// solve
			const auto decomposition = lup_decomposition<6, scalar>::compute(h);
			const scalar det = decomposition.determinant();
			if (!std::isfinite(det) || std::abs(det) < 1e-6) {
				log().debug("Indefinite Hessian");
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

	void solver::_update_body_dual_variables(_body_step_data &bdata) {
		for (usize ci = 0; ci < contacts.size(); ++ci) {
			const constraints::rigid_body_contact &contact = contacts[ci];
			_contact_dual &dual = bdata.contact_duals[ci];
			for (usize cpi = 0; cpi < contact.contact_points.size(); ++cpi) {
				const constraints::rigid_body_contact::point &contact_point = contact.contact_points[cpi];
				_contact_dual::point &dual_point = dual.contact_points[cpi];

				const vec3 c = _compute_contact_error(contact, contact_point, dual_point);
				const vec3 unclamped_force = matm::multiply(dual_point.stiffness, c) + dual_point.force;
				const auto force =
					_contact_force::clamp(unclamped_force, contact.body1->material, contact.body2->material);
				dual_point.force = force.force;
				if (!force.normal_clamped) {
					dual_point.stiffness[0] += stiffness_ramping * std::abs(c[0]);
				}
				if (!force.friction_clamped) {
					dual_point.stiffness += vec3(0.0f, stiffness_ramping * matm::abs(c.block<2, 1>(1, 0)));
				}
				dual_point.stiffness = matm::min(dual_point.stiffness, vec3::filled(maximum_stiffness));
			}
		}

		for (usize ci = 0; ci < pins.size(); ++ci) {
			const constraints::pin &pin = pins[ci];
			_pin_dual &dual = bdata.pin_duals[ci];

			const vec3 c = pin.get_global_position1() - pin.get_global_position2();
			dual.force = dual.force + matm::multiply(dual.stiffness, c);
			dual.stiffness = matm::min(dual.stiffness + matm::abs(c), vec3::filled(maximum_stiffness));
		}

		for (usize ci = 0; ci < hinges.size(); ++ci) {
			const constraints::hinge &hinge = hinges[ci];
			_hinge_dual &dual = bdata.hinge_duals[ci];

			const scalar c = hinge_constant - vec::dot(hinge.get_global_axis1(), hinge.get_global_axis2());
			dual.force = dual.force + dual.stiffness * c;
			dual.stiffness = std::min(dual.stiffness + std::abs(c), maximum_stiffness);
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
