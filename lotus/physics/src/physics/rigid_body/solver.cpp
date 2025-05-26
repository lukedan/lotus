#include "lotus/physics/rigid_body/solver.h"

/// \file
/// Implementation of the rigid body solver.

#include "lotus/physics/world.h"

namespace lotus::physics::rigid_body {
	void solver::timestep(scalar dt, u32 iters) {
		// advect
		for (body *b : physics_world->get_bodies()) {
			b->position_integration(dt);
			b->velocity_integration(dt, physics_world->gravity, zero);
		}

		// handle collisions
		contact_constraints.clear();
		{
			const std::vector<world::collision_info> collisions = physics_world->detect_collisions();
			std::vector<body*> bodies;
			std::vector<constraints::contact_set_blcp::contact_info> contacts;
			std::unordered_map<body*, u32> body_ids;
			auto get_body_id = [&](body *b) {
				auto [it, inserted] = body_ids.emplace(b, static_cast<u32>(bodies.size()));
				if (inserted) {
					bodies.emplace_back(b);
				}
				return it->second;
			};
			for (const world::collision_info &ci : collisions) {
				constraints::contact_set_blcp::contact_info &contact = contacts.emplace_back(uninitialized);
				contact.tangents = tangent_frame<scalar>::from_normal(ci.contact.normal);
				contact.contact  = ci.body1->state.position.local_to_global(ci.contact.contact1);
				contact.body1    = get_body_id(ci.body1);
				contact.body2    = get_body_id(ci.body2);
			}
			contact_constraints.emplace_back(constraints::contact_set_blcp::create(bodies, contacts));
		}

		for (u32 i = 0; i < iters; ++i) {
			for (constraints::contact_set_blcp &constraint : contact_constraints) {
				constraint.solve_iteration(dt);
			}
		}
		for (constraints::contact_set_blcp &constraint : contact_constraints) {
			constraint.apply_impulses();
		}
	}
}
