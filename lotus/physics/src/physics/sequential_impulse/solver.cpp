#include "lotus/physics/solvers/sequential_impulse/solver.h"

/// \file
/// Implementation of the rigid body solver.

#include "lotus/profiler.h"

#include "lotus/physics/world.h"

namespace lotus::physics::solvers::sequential_impulse {
#pragma region "Contact Constraints"
	vec3 solver::_contact_constraint_data::point_data::update_lambda(
		vec3 tangential_velocity, const constraints::rigid_body_contact &contact
	) {
		const vec3 old_lambda = lambda;
		const vec3 delta = -vec3(
			inv_effective_mass_n * tangential_velocity[0],
			inv_effective_mass_t * tangential_velocity.subvector<2>(1)
		);
		lambda += delta;

		// clamp normal impulse
		lambda[0] = std::min<scalar>(lambda[0], 0.0f) - stabilization * inv_effective_mass_n;

		// clamp friction impulse
		const scalar static_friction =
			std::min(contact.body1->material.static_friction, contact.body2->material.static_friction);
		const scalar dynamic_friction =
			std::min(contact.body1->material.dynamic_friction, contact.body2->material.dynamic_friction);
		const scalar max_static_friction_force = static_friction * std::max(0.0f, -lambda[0]);
		const vec2 tangential_force = lambda.subvector<2>(1);
		const scalar tangential_force_mag = tangential_force.norm();
		if (tangential_force_mag > max_static_friction_force) {
			const vec2 clamped_force =
				tangential_force * (dynamic_friction * std::max(0.0f, -lambda[0]) / tangential_force_mag);
			lambda.set_subvector(1, clamped_force);
		}

		return lambda - old_lambda;
	}

	void solver::_contact_constraint_data::point_data::apply_impulses_for_delta_lambda(
		vec3 delta_lambda, const constraints::rigid_body_contact &contact, const _contact_constraint_data &cdata
	) const {
		const vec3 impulse = contact.tangents.get_tangent_to_world_matrix() * delta_lambda;
		_apply_impulses(
			contact.body1,
			contact.body2,
			cdata.inverse_inertia1,
			cdata.inverse_inertia2,
			offset1,
			offset2,
			impulse
		);
	}


	void solver::_contact_constraint_data::prepare(
		const constraints::rigid_body_contact &contact, scalar baumgarte_coeff, scalar collision_threshold
	) {
		const scalar sum_inv_mass = contact.body1->properties.inverse_mass + contact.body2->properties.inverse_mass;

		const mat33s rot1 = contact.body1->state.position.orientation.into_rotation_matrix();
		const mat33s rot2 = contact.body2->state.position.orientation.into_rotation_matrix();
		inverse_inertia1 = rot1 * contact.body1->properties.inverse_inertia * rot1.transposed();
		inverse_inertia2 = rot2 * contact.body2->properties.inverse_inertia * rot2.transposed();

		points.resize(contact.contact_points.size(), zero);
		for (usize pi = 0; pi < contact.contact_points.size(); ++pi) {
			const constraints::rigid_body_contact::point &point = contact.contact_points[pi];
			point_data &pdata = points[pi];
			pdata.offset1 = contact.body1->state.position.orientation.rotate(point.local_position1);
			pdata.offset2 = contact.body2->state.position.orientation.rotate(point.local_position2);
			{ // compute normal effective mass
				const scalar inv_m1 = contact.body1->properties.inverse_mass;
				const scalar inv_m2 = contact.body2->properties.inverse_mass;
				const vec3 n = contact.tangents.normal;
				const vec3 sum_inertia_term =
					inverse_inertia1 * vec::cross(vec::cross(pdata.offset1, n), pdata.offset1) +
					inverse_inertia2 * vec::cross(vec::cross(pdata.offset2, n), pdata.offset2);
				pdata.inv_effective_mass_n = 1.0f / (inv_m1 + inv_m2 + vec::dot(sum_inertia_term, n));
			}
			{ // compute tangential effective mass
				const matrix<3, 2, scalar> n =
					mat::concat_columns(contact.tangents.tangent, contact.tangents.bitangent);
				const mat33s r1x = vec::cross_matrix(pdata.offset1);
				const mat33s r2x = vec::cross_matrix(pdata.offset2);
				const mat33s sum_inertia_term =
					r1x * contact.body1->properties.inverse_inertia * r1x +
					r2x * contact.body2->properties.inverse_inertia * r2x;
				pdata.inv_effective_mass_t =
					(sum_inv_mass * mat22s::identity() - n.transposed() * sum_inertia_term * n).inverse();
			}
			const scalar penetration_depth = std::max(0.0f, vec::dot(
				contact.tangents.normal,
				(contact.body1->state.position.position + pdata.offset1) -
				(contact.body2->state.position.position + pdata.offset2)
			) - collision_threshold);
			pdata.stabilization = baumgarte_coeff * penetration_depth;
		}
	}

	void solver::_contact_constraint_data::velocity_update(const constraints::rigid_body_contact &contact) {
		for (usize pi = 0; pi < points.size(); ++pi) {
			point_data &pdata = points[pi];

			const vec3 rel_velocity =
				contact.body1->state.velocity.get_velocity_at(pdata.offset1) -
				contact.body2->state.velocity.get_velocity_at(pdata.offset2);
			const vec3 tangential_velocity = contact.tangents.get_world_to_tangent_matrix() * rel_velocity;
			const vec3 delta_lambda = pdata.update_lambda(tangential_velocity, contact);
			pdata.apply_impulses_for_delta_lambda(delta_lambda, contact, *this);
		}
	}
#pragma endregion


#pragma region "Hinge Constraints"
	void solver::_hinge_constraint_data::prepare(const constraints::hinge &hinge, scalar baumgarte_coeff) {
		axis1 = hinge.get_global_axis1();
		axis2 = hinge.get_global_axis2();
		inverse_inertia1 = hinge.body1 ? hinge.body1->get_rotated_inverse_inertia() : zero;
		inverse_inertia2 = hinge.body2 ? hinge.body2->get_rotated_inverse_inertia() : zero;

		const mat33s sum_r_invi =
			vec::cross_matrix(axis1) * inverse_inertia1 +
			vec::cross_matrix(axis2) * inverse_inertia2;
		// TODO use sum of inverse mass to modulate regularization term?
		inv_effective_mass =
			(sum_r_invi.transposed() * sum_r_invi + mat33s::identity()).inverse() * sum_r_invi.transposed();
		stabilization = baumgarte_coeff * (axis1 - axis2);
	}

	void solver::_hinge_constraint_data::velocity_update(const constraints::hinge &hinge) {
		const vec3 rel_velocity =
			vec::cross(hinge.body1 ? hinge.body1->state.velocity.angular : zero, axis1) -
			vec::cross(hinge.body2 ? hinge.body2->state.velocity.angular : zero, axis2);
		const vec3 delta_lambda = inv_effective_mass * (rel_velocity + stabilization);
		lambda += delta_lambda;
		apply_impulses_for_delta_lambda(delta_lambda, hinge);
	}

	void solver::_hinge_constraint_data::apply_impulses_for_delta_lambda(
		vec3 delta_lambda, const constraints::hinge &hinge
	) const {
		if (hinge.body1) {
			hinge.body1->state.velocity.angular += inverse_inertia1 * delta_lambda;
		}
		if (hinge.body2) {
			hinge.body2->state.velocity.angular += inverse_inertia2 * -delta_lambda;
		}
	}
#pragma endregion


#pragma region "Pin Constraints"
	mat33s solver::_pin_constraint_data::compute_effective_mass(
		mat33s inv_i1, mat33s inv_i2, scalar inv_m1, scalar inv_m2, vec3 o1, vec3 o2
	) {
		const mat33s r1x = vec::cross_matrix(o1);
		const mat33s r2x = vec::cross_matrix(o2);
		return mat33s::identity() * (inv_m1 + inv_m2) - r1x * inv_i1 * r1x - r2x * inv_i2 * r2x;
	}

	void solver::_pin_constraint_data::prepare(const constraints::pin &pin, scalar baumgarte_coeff) {
		if (pin.body1) {
			offset1 = pin.body1->state.position.orientation.rotate(pin.local_position1);
			inverse_inertia1 = pin.body1->get_rotated_inverse_inertia();
		} else {
			offset1 = pin.local_position1;
		}
		if (pin.body2) {
			offset2 = pin.body2->state.position.orientation.rotate(pin.local_position2);
			inverse_inertia2 = pin.body2->get_rotated_inverse_inertia();
		} else {
			offset2 = pin.local_position2;
		}
		inv_effective_mass = compute_effective_mass(
			inverse_inertia1,
			inverse_inertia2,
			pin.body1 ? pin.body1->properties.inverse_mass : 0.0f,
			pin.body2 ? pin.body2->properties.inverse_mass : 0.0f,
			offset1,
			offset2
		).inverse();
		const vec3 diff = pin.get_global_position1() - pin.get_global_position2();
		stabilization = diff * baumgarte_coeff;
	}

	void solver::_pin_constraint_data::velocity_update(const constraints::pin &pin) {
		const vec3 rel_velocity =
			(pin.body1 ? pin.body1->state.velocity.get_velocity_at(offset1) : zero) -
			(pin.body2 ? pin.body2->state.velocity.get_velocity_at(offset2) : zero);

		const vec3 delta_lambda = inv_effective_mass * -(rel_velocity + stabilization);
		lambda += delta_lambda;
		apply_impulses_for_delta_lambda(delta_lambda, pin);
	}

	void solver::_pin_constraint_data::apply_impulses_for_delta_lambda(
		vec3 delta_lambda, const constraints::pin &pin
	) const {
		_apply_impulses(pin.body1, pin.body2, inverse_inertia1, inverse_inertia2, offset1, offset2, delta_lambda);
	}
#pragma endregion


	void solver::timestep(scalar full_dt) {
		profiler::scope p1;

		const scalar substep_dt = full_dt / static_cast<scalar>(num_substeps);
		const scalar baumgarte_coeff = baumgarte_stabilization;

		physics_world->update_contact_constraints();

		// TODO ugly
		u32 num_contacts = 0;
		physics_world->for_each_contact([&](const constraints::rigid_body_contact&) {
			++num_contacts;
		});

		std::vector<_contact_constraint_data> contact_data(num_contacts, zero);
		std::vector<_hinge_constraint_data> hinge_data(physics_world->hinges.size(), zero);
		std::vector<_pin_constraint_data> pin_data(physics_world->pins.size(), zero);

		for (u32 substep = 0; substep < num_substeps; ++substep) {
			// advect
			physics_world->for_each_body([&](body *b) {
				if (substep == 0) {
					b->state.velocity.linear += b->applied_impulse * b->properties.inverse_mass;
					b->state.velocity.angular += b->state.position.orientation.rotate(
						b->properties.inverse_inertia * b->state.position.orientation.conjugate().rotate(b->applied_torque)
					);
					b->applied_impulse = zero;
					b->applied_torque = zero;
				}

				b->velocity_integration(substep_dt, physics_world->gravity, zero);
			});

			// apply spring forces
			// TODO implicit formulation
			for (const constraints::spring &spring : physics_world->springs) {
				const vec3 diff = spring.get_global_position1() - spring.get_global_position2();
				const scalar diff_len = diff.norm();
				const scalar stretch = diff_len - spring.initial_length;
				const scalar force_mag =
					(stretch > 0.0f ? spring.stretched_stiffness : spring.compressed_stiffness) * stretch;
				const vec3 impulse = diff * (substep_dt * force_mag / diff_len);
				if (spring.body1) {
					spring.body1->apply_impulse_immediate(
						spring.body1->state.position.orientation.rotate(spring.local_position1), -impulse
					);
				}
				if (spring.body2) {
					spring.body2->apply_impulse_immediate(
						spring.body2->state.position.orientation.rotate(spring.local_position2), impulse
					);
				}
			}

			{
				profiler::scope p2(u8"Prepare Constraints");
				u32 contact_index = 0;
				physics_world->for_each_contact([&](const constraints::rigid_body_contact &contact) {
					contact_data[contact_index].prepare(
						contact, baumgarte_coeff, physics_world->collision_threshold
					);
					++contact_index;
				});
				for (usize hi = 0; hi < physics_world->hinges.size(); ++hi) {
					const constraints::hinge &hinge = physics_world->hinges[hi];
					hinge_data[hi].prepare(hinge, baumgarte_coeff);
				}
				for (usize pi = 0; pi < physics_world->pins.size(); ++pi) {
					const constraints::pin &pin = physics_world->pins[pi];
					pin_data[pi].prepare(pin, baumgarte_coeff);
				}
			}

			if (substep > 0) {
				profiler::scope p2(u8"Apply Initial Guess");
				u32 contact_index = 0;
				physics_world->for_each_contact([&](const constraints::rigid_body_contact &contact) {
					const _contact_constraint_data &cdata = contact_data[contact_index];
					for (const _contact_constraint_data::point_data &point : cdata.points) {
						point.apply_impulses_for_delta_lambda(point.lambda, contact, cdata);
					}
					++contact_index;
				});
				for (usize hi = 0; hi < physics_world->hinges.size(); ++hi) {
					const constraints::hinge &hinge = physics_world->hinges[hi];
					const _hinge_constraint_data &hdata = hinge_data[hi];
					hdata.apply_impulses_for_delta_lambda(hdata.lambda, hinge);
				}
				for (usize pi = 0; pi < physics_world->pins.size(); ++pi) {
					const constraints::pin &pin = physics_world->pins[pi];
					const _pin_constraint_data &pdata = pin_data[pi];
					pdata.apply_impulses_for_delta_lambda(pdata.lambda, pin);
				}
			}

			for (u32 iter = 0; iter < num_velocity_iterations; ++iter) {
				profiler::scope p2(u8"Velocity Iteration");
				{
					usize ci = 0;
					physics_world->for_each_contact([&](const constraints::rigid_body_contact &contact) {
						contact_data[ci].velocity_update(contact);
						++ci;
					});
				}
				for (usize ci = 0; ci < physics_world->hinges.size(); ++ci) {
					hinge_data[ci].velocity_update(physics_world->hinges[ci]);
				}
				for (usize ci = 0; ci < physics_world->pins.size(); ++ci) {
					pin_data[ci].velocity_update(physics_world->pins[ci]);
				}
			}

			physics_world->for_each_body([&](body *b) {
				if (b->properties.inverse_mass > 0.0f) {
					b->position_integration(substep_dt);
					if (substep + 1 == num_substeps) {
						physics_world->on_body_moved(b);
					}
				}
			});
		}
	}

	void solver::_apply_impulses(
		body *body1, body *body2, mat33s inv_i1, mat33s inv_i2, vec3 off1, vec3 off2, vec3 impulse
	) {
		if (body1) {
			body1->state.velocity.linear += body1->properties.inverse_mass * impulse;
			body1->state.velocity.angular += inv_i1 * vec::cross(off1, impulse);
		}
		if (body2) {
			body2->state.velocity.linear += body2->properties.inverse_mass * -impulse;
			body2->state.velocity.angular += inv_i2 * vec::cross(off2, -impulse);
		}
	}
}
