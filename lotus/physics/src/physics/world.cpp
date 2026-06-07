#include "lotus/physics/world.h"

/// \file
/// Implementation of the physics world.

#include "lotus/collision/algorithms/gjk.h"
#include "lotus/collision/algorithms/epa.h"

namespace lotus::physics {
	std::vector<world::rigid_body_collision> world::detect_collisions() const {
		std::vector<rigid_body_collision> result;
		for (size_t i = 0; i < _bodies.size(); ++i) {
			for (size_t j = i + 1; j < _bodies.size(); ++j) {
				body *body1 = _bodies[i];
				body *body2 = _bodies[j];
				if (body1->body_shape->get_type() > body2->body_shape->get_type()) {
					std::swap(body1, body2);
				}
				const std::optional<collision::rigid_body_contact> col = collision::rigid_body_contact::detect(
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
