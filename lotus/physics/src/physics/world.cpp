#include "lotus/physics/world.h"

/// \file
/// Implementation of the physics world.

#include "lotus/collision/algorithms/gjk.h"
#include "lotus/collision/algorithms/epa.h"
#include "lotus/collision/algorithms/contact_manifold.h"

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
				if (
					body1->body_shape->get_type() == collision::shape::type::polyhedron &&
					body2->body_shape->get_type() == collision::shape::type::polyhedron
				) {
					// special case: construct full collision manifold
					const collision::polyhedron_pair pair(
						std::get<collision::shapes::convex_polyhedron>(body1->body_shape->value),
						body1->state.position,
						std::get<collision::shapes::convex_polyhedron>(body2->body_shape->value),
						body2->state.position
					);

					const collision::gjk::result gjk_res = collision::gjk::gjk(pair);
					if (!gjk_res.has_intersection) {
						continue;
					}
					const collision::epa::result epa_res = collision::epa::epa(pair, gjk_res);
					const collision::contact_manifold mainfold = collision::contact_manifold::compute(pair, epa_res);
					for (const collision::contact_manifold::point &pt : mainfold.points) {
						// TODO simplified storage
						result.emplace_back(
							*body1, *body2,
							collision::rigid_body_contact(pt.local_position1, pt.local_position2, epa_res.normal)
						);
					}
				} else {
					const std::optional<collision::rigid_body_contact> col = collision::rigid_body_contact::detect(
						*body1->body_shape, body1->state.position, *body2->body_shape, body2->state.position
					);
					if (col) {
						result.emplace_back(*body1, *body2, col.value());
					}
				}
			}
		}
		return result;
	}
}
