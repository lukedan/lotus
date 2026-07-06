#include "lotus/physics/solvers/sequential_impulse/solver.h"

/// \file
/// Implementation of the rigid body solver.

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


	scalar solver::_contact_constraint_data::compute_effective_mass(
		mat33s inv_i1, mat33s inv_i2, scalar inv_m1, scalar inv_m2, vec3 o1, vec3 o2, vec3 axis
	) {
		const vec3 inertia_term =
			inv_i1 * vec::cross(vec::cross(o1, axis), o1) + inv_i2 * vec::cross(vec::cross(o2, axis), o2);
		return inv_m1 + inv_m2 + vec::dot(inertia_term, axis);
	}

	scalar solver::_contact_constraint_data::compute_effective_mass(
		const constraints::rigid_body_contact &contact,
		const _contact_constraint_data &cdata,
		const point_data &pdata,
		vec3 axis
	) {
		return compute_effective_mass(
			cdata.inverse_inertia1,
			cdata.inverse_inertia2,
			contact.body1->properties.inverse_mass,
			contact.body2->properties.inverse_mass,
			pdata.offset1,
			pdata.offset2,
			axis
		);
	}

	solver::_contact_constraint_data solver::_contact_constraint_data::prepare(
		const constraints::rigid_body_contact &contact, scalar baumgarte_coeff, scalar collision_threshold
	) {
		_contact_constraint_data result;

		const scalar sum_inv_mass = contact.body1->properties.inverse_mass + contact.body2->properties.inverse_mass;

		const mat33s rot1 = contact.body1->state.position.orientation.into_rotation_matrix();
		const mat33s rot2 = contact.body2->state.position.orientation.into_rotation_matrix();
		result.inverse_inertia1 = rot1 * contact.body1->properties.inverse_inertia * rot1.transposed();
		result.inverse_inertia2 = rot2 * contact.body2->properties.inverse_inertia * rot2.transposed();

		result.points.reserve(contact.contact_points.size());
		for (const constraints::rigid_body_contact::point &point : contact.contact_points) {
			point_data &pdata = result.points.emplace_back(zero);
			pdata.offset1 = contact.body1->state.position.orientation.rotate(point.local_position1);
			pdata.offset2 = contact.body2->state.position.orientation.rotate(point.local_position2);
			pdata.inv_effective_mass_n =
				1.0f / compute_effective_mass(contact, result, pdata, contact.tangents.normal);
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
			pdata.stabilization = baumgarte_coeff * (vec::dot(
				contact.tangents.normal,
				(contact.body1->state.position.position + pdata.offset1) -
				(contact.body2->state.position.position + pdata.offset2)
			) - collision_threshold);
		}

		return result;
	}

	void solver::_contact_constraint_data::velocity_update(const constraints::rigid_body_contact &contact) {
		for (usize pi = 0; pi < points.size(); ++pi) {
			point_data &pdata = points[pi];

			const vec3 rel_velocity =
				contact.body1->state.velocity.get_velocity_at(pdata.offset1) -
				contact.body2->state.velocity.get_velocity_at(pdata.offset2);
			const vec3 tangential_velocity = contact.tangents.get_world_to_tangent_matrix() * rel_velocity;
			const vec3 delta_lambda = pdata.update_lambda(tangential_velocity, contact);
			const vec3 impulse = contact.tangents.get_tangent_to_world_matrix() * delta_lambda;
			_apply_impulses(
				contact.body1,
				contact.body2,
				inverse_inertia1,
				inverse_inertia2,
				pdata.offset1,
				pdata.offset2,
				impulse
			);
		}
	}
#pragma endregion


#pragma region "Hinge Constraints"
	solver::_hinge_constraint_data solver::_hinge_constraint_data::prepare(
		const constraints::hinge &hinge, scalar baumgarte_coeff
	) {
		_hinge_constraint_data result;

		result.axis1 = hinge.get_global_axis1();
		result.axis2 = hinge.get_global_axis2();
		result.inverse_inertia1 = hinge.body1 ? hinge.body1->get_rotated_inverse_inertia() : zero;
		result.inverse_inertia2 = hinge.body2 ? hinge.body2->get_rotated_inverse_inertia() : zero;

		const mat33s sum_r_invi =
			vec::cross_matrix(result.axis1) * result.inverse_inertia1 +
			vec::cross_matrix(result.axis2) * result.inverse_inertia2;
		// TODO use sum of inverse mass to modulate regularization term?
		result.inv_effective_mass =
			(sum_r_invi.transposed() * sum_r_invi + mat33s::identity()).inverse() * sum_r_invi.transposed();
		result.stabilization = baumgarte_coeff * (result.axis1 - result.axis2);

		return result;
	}

	void solver::_hinge_constraint_data::velocity_update(const constraints::hinge &hinge) {
		const vec3 rel_velocity =
			vec::cross(hinge.body1 ? hinge.body1->state.velocity.angular : zero, axis1) -
			vec::cross(hinge.body2 ? hinge.body2->state.velocity.angular : zero, axis2);
		const vec3 delta_lambda = inv_effective_mass * (rel_velocity + stabilization);
		lambda += delta_lambda;
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

	solver::_pin_constraint_data solver::_pin_constraint_data::prepare(
		const constraints::pin &pin, scalar baumgarte_coeff
	) {
		_pin_constraint_data result;

		if (pin.body1) {
			result.offset1 = pin.body1->state.position.orientation.rotate(pin.local_position1);
			result.inverse_inertia1 = pin.body1->get_rotated_inverse_inertia();
		} else {
			result.offset1 = pin.local_position1;
		}
		if (pin.body2) {
			result.offset2 = pin.body2->state.position.orientation.rotate(pin.local_position2);
			result.inverse_inertia2 = pin.body2->get_rotated_inverse_inertia();
		} else {
			result.offset2 = pin.local_position2;
		}
		result.inv_effective_mass = compute_effective_mass(
			result.inverse_inertia1,
			result.inverse_inertia2,
			pin.body1 ? pin.body1->properties.inverse_mass : 0.0f,
			pin.body2 ? pin.body2->properties.inverse_mass : 0.0f,
			result.offset1,
			result.offset2
		).inverse();
		const vec3 diff = pin.get_global_position1() - pin.get_global_position2();
		result.stabilization = diff * baumgarte_coeff;

		return result;
	}

	void solver::_pin_constraint_data::velocity_update(const constraints::pin &pin) {
		const vec3 rel_velocity =
			(pin.body1 ? pin.body1->state.velocity.get_velocity_at(offset1) : zero) -
			(pin.body2 ? pin.body2->state.velocity.get_velocity_at(offset2) : zero);

		const vec3 delta_lambda = inv_effective_mass * -(rel_velocity + stabilization);
		lambda += delta_lambda;
		_apply_impulses(pin.body1, pin.body2, inverse_inertia1, inverse_inertia2, offset1, offset2, delta_lambda);
	}
#pragma endregion


	void solver::timestep(scalar dt) {
		const scalar rcp_dt = 1.0f / dt;
		const scalar baumgarte_coeff = baumgarte_stabilization * rcp_dt;

		// advect
		for (body *b : physics_world->get_bodies()) {
			b->state.velocity.linear += b->applied_impulse * b->properties.inverse_mass;
			b->state.velocity.angular += b->state.position.orientation.rotate(
				b->properties.inverse_inertia * b->state.position.orientation.conjugate().rotate(b->applied_torque)
			);
			b->applied_impulse = zero;
			b->applied_torque = zero;

			b->velocity_integration(dt, physics_world->gravity, zero);
		}

		// apply spring forces
		// TODO implicit formulation
		for (const constraints::spring &spring : physics_world->springs) {
			const vec3 diff = spring.get_global_position1() - spring.get_global_position2();
			const scalar diff_len = diff.norm();
			const scalar stretch = diff_len - spring.initial_length;
			const scalar force_mag =
				(stretch > 0.0f ? spring.stretched_stiffness : spring.compressed_stiffness) * stretch;
			const vec3 impulse = diff * (dt * force_mag / diff_len);
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

		physics_world->update_contact_constraints();

		std::vector<_contact_constraint_data> contact_data;
		std::vector<_hinge_constraint_data> hinge_data;
		std::vector<_pin_constraint_data> pin_data;
		for (const constraints::rigid_body_contact &contact : physics_world->contacts) {
			contact_data.emplace_back(_contact_constraint_data::prepare(
				contact, baumgarte_coeff, physics_world->collision_threshold
			));
		}
		for (const constraints::hinge &hinge : physics_world->hinges) {
			hinge_data.emplace_back(_hinge_constraint_data::prepare(hinge, baumgarte_coeff));
		}
		for (const constraints::pin &pin : physics_world->pins) {
			pin_data.emplace_back(_pin_constraint_data::prepare(pin, baumgarte_coeff));
		}

		for (u32 iter = 0; iter < num_velocity_iterations; ++iter) {
			for (usize ci = 0; ci < physics_world->contacts.size(); ++ci) {
				contact_data[ci].velocity_update(physics_world->contacts[ci]);
			}
			for (usize ci = 0; ci < physics_world->hinges.size(); ++ci) {
				hinge_data[ci].velocity_update(physics_world->hinges[ci]);
			}
			for (usize ci = 0; ci < physics_world->pins.size(); ++ci) {
				pin_data[ci].velocity_update(physics_world->pins[ci]);
			}
		}

		for (body *b : physics_world->get_bodies()) {
			b->position_integration(dt);
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
