#include "lotus/physics/world.h"

/// \file
/// Implementation of the physics world.

#include "lotus/collision/algorithms/contact_manifold.h"
#include "lotus/collision/contact.h"

namespace lotus::physics {
	std::vector<world::rigid_body_collision> world::detect_collisions() const {
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

				body *body1 = body_i;
				body *body2 = body_j;
				if (body1->body_shape->get_type() > body2->body_shape->get_type()) {
					std::swap(body1, body2);
				}
				const std::optional<collision::contact_manifold> col = collision::contact::detect(
					*body1->body_shape, body1->state.position, *body2->body_shape, body2->state.position
				);
				if (col) {
					result.emplace_back(*body1, *body2, col.value());
				}
			}
		}
		return result;
	}
}
