#include "lotus/physics/world.h"

/// \file
/// Implementation of the physics world.

#include <set>

#include "lotus/profiler.h"
#include "lotus/collision/algorithms/contact_manifold.h"
#include "lotus/collision/contact.h"

namespace lotus::physics {
	std::vector<world::rigid_body_collision> world::detect_collisions() const {
		// collect pairs of bodies that have collision disabled explicitly
		// TODO this is suboptimal - need perfect ordering between bodies
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

		std::vector<rigid_body_collision> result;
		for (size_t i = 0; i < _bodies.size(); ++i) {
			body *const body_i = _bodies[i];
			const bool kinematic_i = body_i->properties.inverse_mass <= 0.0f;
			for (size_t j = i + 1; j < _bodies.size(); ++j) {
				body *const body_j = _bodies[j];
				const bool kinematic_j = body_j->properties.inverse_mass <= 0.0f;
				if (kinematic_i && kinematic_j) {
					continue;
				}
				if (collision_disabled.contains(std::make_pair(body_i, body_j))) {
					continue;
				}

				body *body1 = body_i;
				body *body2 = body_j;
				if (body1->body_shape->get_type() > body2->body_shape->get_type()) {
					std::swap(body1, body2);
				}
				const std::optional<collision::contact_manifold> col = collision::contact::detect(
					*body1->body_shape, body1->state.position, *body2->body_shape, body2->state.position
				);
				if (col && !col->points.empty()) {
					result.emplace_back(*body1, *body2, col.value());
				}
			}
		}
		return result;
	}

	void world::update_contact_constraints() {
		profiler::scope p1;

		// TODO get rid of detect_collisions() and update the constraints array directly
		const std::vector<rigid_body_collision> collisions = detect_collisions();
		// TODO persistence
		contacts.clear();
		contacts.reserve(collisions.size());
		for (const rigid_body_collision &col : collisions) {
			constraints::rigid_body_contact &constraint = contacts.emplace_back();
			constraint.body1 = col.body1;
			constraint.body2 = col.body2;
			constraint.tangents = tangent_frame<scalar>::from_normal(col.contact_manifold.normal);
			for (const collision::contact_manifold::point &manifold_pt : col.contact_manifold.points) {
				constraints::rigid_body_contact::point &pt = constraint.contact_points.emplace_back();
				pt.local_position1 = manifold_pt.local_position1;
				pt.local_position2 = manifold_pt.local_position2;
			}
		}
	}
}
