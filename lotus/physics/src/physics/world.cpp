#include "lotus/physics/world.h"

/// \file
/// Implementation of the physics world.

#include "lotus/logging.h"
#include "lotus/profiler.h"
#include "lotus/collision/algorithms/contact_manifold.h"
#include "lotus/collision/contact.h"

namespace lotus::physics {
	void world::overlap_data::update_contact(body *body1, body *body2) {
		if (body1->body_shape->get_type() > body2->body_shape->get_type()) {
			std::swap(body1, body2);
		}
		const std::optional<collision::contact_manifold> col = collision::contact::detect(
			*body1->body_shape, body1->state.position, *body2->body_shape, body2->state.position
		);
		if (col && !col->points.empty()) {
			constraints::rigid_body_contact &constraint = contact.emplace();
			constraint.body1 = body1;
			constraint.body2 = body2;
			constraint.tangents = tangent_frame<scalar>::from_normal(col->normal);
			for (const collision::contact_manifold::point &manifold_pt : col->points) {
				constraints::rigid_body_contact::point &pt = constraint.contact_points.emplace_back();
				pt.local_position1 = manifold_pt.local_position1;
				pt.local_position2 = manifold_pt.local_position2;
			}
		} else {
			contact.reset();
		}
	}


	void world::update_contact_constraints() {
		profiler::scope p1;

		++_timestamp;

		// collect pairs of bodies that have collision disabled explicitly
		std::unordered_set<body_pair, body_pair_hash> collision_disabled;
		for (const constraints::spring &spring : springs) {
			if (spring.disable_collision && spring.body1 && spring.body2) {
				collision_disabled.emplace(spring.body1, spring.body2);
			}
		}
		for (const constraints::pin &pin : pins) {
			if (pin.disable_collision && pin.body1 && pin.body2) {
				collision_disabled.emplace(pin.body1, pin.body2);
			}
		}
		for (const constraints::hinge &hinge : hinges) {
			if (hinge.disable_collision && hinge.body1 && hinge.body2) {
				collision_disabled.emplace(hinge.body1, hinge.body2);
			}
		}

		{ // first detach everything from the tree and remove existing overlap pairs
			profiler::scope p2(u8"Erase Old AABBs");

			for (body *cur : _bodies_to_update) {
				body_data &bdata = _body_map.at(cur);
				_body_bvh.detach(bdata.node);
				for (body *other : bdata.overlaps) {
					_overlap.erase(body_pair(cur, other));
					_body_map.at(other).overlaps.erase(cur);
				}
				bdata.overlaps.clear();
			}
		}

		{ // then re-insert everything into the tree with updated AABBs
			profiler::scope p2(u8"Reinsert Updated AABBs");

			for (body *cur : _bodies_to_update) {
				const bool cur_kinematic = cur->properties.inverse_mass <= 0.0f;
				body_data &bdata = _body_map.at(cur);

				// find new overlap pairs and collisions
				{
					profiler::scope p3(u8"BVH Query");
					_body_bvh.query_aab(bdata.aabb, [&](const body_bvh::leaf_node *other_node) {
						body *other = other_node->value;
						if (cur_kinematic && other->properties.inverse_mass <= 0.0f) {
							return; // no collision between kinematic bodies
						}
						if (collision_disabled.contains(body_pair(cur, other))) {
							return;
						}

						bdata.overlaps.emplace(other);
						_body_map.at(other).overlaps.emplace(cur);
						auto [overlap_it, inserted] = _overlap.emplace(body_pair(cur, other), overlap_data());
						crash_if(!inserted);
					});
				}

				// insert into BVH
				_body_bvh.insert(bdata.node, bdata.aabb);
			}
		}

		{ // finally, update all existing contacts
			profiler::scope p2(u8"Detect Collisions");

			for (auto it = _overlap.begin(); it != _overlap.end(); ++it) {
				it->second.update_contact(it->first);
			}
		}

		_bodies_to_update.clear();
	}

	void world::on_body_moved(body *b) {
		const aab3s tight_aab = b->body_shape->get_aabb_with_transform(b->state.position);
		body_data &bdata = _body_map.at(b);
		if (!bdata.aabb.contains(tight_aab)) {
			bdata.set_aabb(_get_expanded_aab(tight_aab, b->state.velocity.linear), _timestamp);
			_bodies_to_update.emplace(b);
		}
	}

	void world::_maybe_validate_bvh() const {
		if constexpr (validate_bvh) {
			_body_bvh.validate([](const body_bvh::node *n, const char8_t *msg) {
				log().error("Node {:x}: {}", reinterpret_cast<intptr_t>(n), reinterpret_cast<const char*>(msg));
				pause_for_debugger();
			});
		}
	}
}
