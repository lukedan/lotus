#include "lotus/physics/solvers/sequential_impulse/solver.h"

/// \file
/// Implementation of the rigid body solver.

#include "lotus/physics/world.h"

namespace lotus::physics::solvers::sequential_impulse {
	vec3 solver::_contact_constraint_data::point_data::update_lambda(
		vec3 delta, const constraints::rigid_body_contact &contact
	) {
		const scalar friction_coefficient =
			std::min(contact.body1->material.dynamic_friction, contact.body2->material.dynamic_friction);
		const vec3 old_lambda = lambda;
		lambda += delta;
		lambda[0] = std::max(lambda[0], 0.0f) + stabilization / effective_mass[0];
		const scalar max_friction_force = friction_coefficient * lambda[0];
		lambda[1] = std::clamp(lambda[1], -max_friction_force, max_friction_force);
		lambda[2] = std::clamp(lambda[2], -max_friction_force, max_friction_force);
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
		const constraints::rigid_body_contact &contact, scalar baumgarte, scalar rcp_dt
	) {
		_contact_constraint_data result;

		const mat33s rot1 = contact.body1->state.position.orientation.into_rotation_matrix();
		const mat33s rot2 = contact.body2->state.position.orientation.into_rotation_matrix();
		result.inverse_inertia1 = rot1 * contact.body1->properties.inverse_inertia * rot1.transposed();
		result.inverse_inertia2 = rot2 * contact.body2->properties.inverse_inertia * rot2.transposed();

		result.points.reserve(contact.contact_points.size());
		for (const constraints::rigid_body_contact::point &point : contact.contact_points) {
			point_data &pdata = result.points.emplace_back(zero);
			pdata.offset1 = contact.body1->state.position.orientation.rotate(point.local_position1);
			pdata.offset2 = contact.body2->state.position.orientation.rotate(point.local_position2);
			pdata.effective_mass = {
				compute_effective_mass(contact, result, pdata, contact.tangents.normal),
				compute_effective_mass(contact, result, pdata, contact.tangents.tangent),
				compute_effective_mass(contact, result, pdata, contact.tangents.bitangent)
			};
			pdata.stabilization = baumgarte * rcp_dt * vec::dot(
				contact.tangents.normal,
				contact.body1->state.position.position + pdata.offset1 -
				(contact.body2->state.position.position + pdata.offset2)
			);
		}

		return result;
	}

	void solver::_contact_constraint_data::apply_impulses(
		const constraints::rigid_body_contact &contact, vec3 off1, vec3 off2, vec3 impulse
	) {
		contact.body1->state.velocity.linear += contact.body1->properties.inverse_mass * -impulse;
		contact.body1->state.velocity.angular += inverse_inertia1 * vec::cross(off1, -impulse);
		contact.body2->state.velocity.linear += contact.body2->properties.inverse_mass * impulse;
		contact.body2->state.velocity.angular += inverse_inertia2 * vec::cross(off2, impulse);
	}

	void solver::_contact_constraint_data::velocity_update(const constraints::rigid_body_contact &contact) {
		for (usize pi = 0; pi < points.size(); ++pi) {
			point_data &pdata = points[pi];

			const vec3 rel_velocity =
				contact.body1->state.velocity.get_velocity_at(pdata.offset1) -
				contact.body2->state.velocity.get_velocity_at(pdata.offset2);
			const vec3 nominator = contact.tangents.get_world_to_tangent_matrix() * rel_velocity;

			const vec3 delta_lambda =
				pdata.update_lambda(matm::divide(nominator, pdata.effective_mass), contact);
			const vec3 impulse = contact.tangents.get_tangent_to_world_matrix() * delta_lambda;
			apply_impulses(contact, pdata.offset1, pdata.offset2, impulse);
		}
	}


	void solver::timestep(scalar dt) {
		const scalar rcp_dt = 1.0f / dt;

		// advect
		for (body *b : physics_world->get_bodies()) {
			b->position_integration(dt);
			b->velocity_integration(dt, physics_world->gravity, zero);
		}

		physics_world->update_contact_constraints();

		std::vector<_contact_constraint_data> contact_data;
		for (const constraints::rigid_body_contact &contact : physics_world->contacts) {
			contact_data.emplace_back(_contact_constraint_data::prepare(contact, baumgarte_stabilization, rcp_dt));
		}

		for (u32 iter = 0; iter < num_velocity_iterations; ++iter) {
			for (usize ci = 0; ci < physics_world->contacts.size(); ++ci) {
				contact_data[ci].velocity_update(physics_world->contacts[ci]);
			}
		}
	}
}
