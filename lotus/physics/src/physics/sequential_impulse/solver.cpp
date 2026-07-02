#include "lotus/physics/solvers/sequential_impulse/solver.h"

/// \file
/// Implementation of the rigid body solver.

#include "lotus/physics/world.h"

namespace lotus::physics::solvers::sequential_impulse {
	void solver::timestep(scalar dt) {
		physics_world->update_contact_constraints();

		std::vector<std::vector<vec3>> lambdas;
		for (const constraints::rigid_body_contact &contact : physics_world->contacts) {
			lambdas.emplace_back(contact.contact_points.size(), zero);
		}
		for (u32 iter = 0; iter < num_iterations; ++iter) {
			for (usize ci = 0; ci < physics_world->contacts.size(); ++ci) {
				const constraints::rigid_body_contact &contact = physics_world->contacts[ci];
				std::vector<vec3> &lambda_array = lambdas[ci];
				for (usize pi = 0; pi < contact.contact_points.size(); ++pi) {
					const constraints::rigid_body_contact::point &contact_point = contact.contact_points[pi];
					vec3 &lambda = lambda_array[pi];

					const vec3 r1 = contact.body1->state.position.orientation.rotate(contact_point.local_position1);
					const vec3 r2 = contact.body2->state.position.orientation.rotate(contact_point.local_position2);
					const mat33s rot1 = contact.body1->state.position.orientation.into_rotation_matrix();
					const mat33s rot2 = contact.body2->state.position.orientation.into_rotation_matrix();
					const mat33s i1 = rot1 * contact.body1->properties.inverse_inertia * rot1.transposed();
					const mat33s i2 = rot2 * contact.body2->properties.inverse_inertia * rot2.transposed();

					const tangent_frame<scalar> ntb = contact.tangents;
					const vec3 rel_velocity =
						contact.body1->state.velocity.get_velocity_at(r1) -
						contact.body2->state.velocity.get_velocity_at(r2);
					const vec3 nominator = ntb.get_world_to_tangent_matrix() * rel_velocity;

					const auto pseudo_mass = [&](vec3 axis) {
						const vec3 inertia_term =
							i1 * vec::cross(vec::cross(r1, axis), r1) + i2 * vec::cross(vec::cross(r2, axis), r2);
						return
							contact.body1->properties.inverse_mass +
							contact.body2->properties.inverse_mass +
							vec::dot(inertia_term, axis);
					};
					const vec3 denominator =
						{ pseudo_mass(ntb.normal), pseudo_mass(ntb.tangent), pseudo_mass(ntb.bitangent) };

					// compute new lambda
					const vec3 old_lambda = lambda;
					lambda += matm::divide(nominator, denominator);
					lambda[0] = std::max(lambda[0], 0.0f);
					const scalar friction_coefficient =
						std::min(contact.body1->material.dynamic_friction, contact.body2->material.dynamic_friction);
					const scalar max_friction_force = friction_coefficient * lambda[0];
					lambda[1] = std::clamp(lambda[1], -max_friction_force, max_friction_force);
					lambda[2] = std::clamp(lambda[2], -max_friction_force, max_friction_force);

					// apply impulses
					const vec3 delta_lambda = lambda - old_lambda;
					const vec3 impulse = ntb.get_tangent_to_world_matrix() * delta_lambda;
					contact.body1->apply_impulse(contact.body1->state.position.position + r1, -impulse);
					contact.body2->apply_impulse(contact.body2->state.position.position + r2, impulse);
				}
			}
		}

		// advect
		for (body *b : physics_world->get_bodies()) {
			b->position_integration(dt);
			b->velocity_integration(dt, physics_world->gravity, zero);
		}
	}
}
