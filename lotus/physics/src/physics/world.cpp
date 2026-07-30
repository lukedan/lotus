#include "lotus/physics/world.h"

/// \file
/// Implementation of the physics world.

#include "lotus/logging.h"
#include "lotus/profiler.h"
#include "lotus/collision/algorithms/contact_manifold.h"
#include "lotus/collision/contact.h"

namespace lotus::physics {
	void world::overlap_data::update_contact(body &b1, body &b2) {
		body *body1 = &b1;
		body *body2 = &b2;
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
		std::vector<std::pair<body*, body*>> collision_disabled;
		{
			for (const constraints::spring &spring : springs) {
				if (spring.disable_collision && spring.body1 && spring.body2) {
					collision_disabled.emplace_back(std::minmax(spring.body1, spring.body2));
				}
			}
			for (const constraints::pin &pin : pins) {
				if (pin.disable_collision && pin.body1 && pin.body2) {
					collision_disabled.emplace_back(std::minmax(pin.body1, pin.body2));
				}
			}
			for (const constraints::hinge &hinge : hinges) {
				if (hinge.disable_collision && hinge.body1 && hinge.body2) {
					collision_disabled.emplace_back(std::minmax(hinge.body1, hinge.body2));
				}
			}
			std::ranges::sort(collision_disabled);
			const auto to_erase = std::ranges::unique(collision_disabled);
			collision_disabled.erase(to_erase.begin(), to_erase.end());
		}
		const auto is_collision_disabled = [&](body_data *lhs, body_data *rhs) {
			const std::pair<body*, body*> bodies = std::minmax(&lhs->this_body, &rhs->this_body);
			auto it = std::ranges::lower_bound(collision_disabled, bodies);
			return it != collision_disabled.end() && *it == bodies;
		};

		std::ranges::stable_sort(_bodies_to_update, [](const _body_aabb_update &lhs, const _body_aabb_update &rhs) {
			return lhs.target->unique_id < rhs.target->unique_id;
		});
		{
			const auto to_erase = std::ranges::unique(
				_bodies_to_update, [](const _body_aabb_update &lhs, const _body_aabb_update &rhs) {
					return lhs.target == rhs.target;
				}
			);
			_bodies_to_update.erase(to_erase.begin(), to_erase.end());
		}

		std::vector<_body_aabb_update> bodies_to_update;
		std::vector<_body_aabb_update> bodies_to_add;
		for (const _body_aabb_update &update : _bodies_to_update) {
			if (update.target->node->get_parent()) {
				bodies_to_update.emplace_back(update);
			} else {
				bodies_to_add.emplace_back(update);
			}
		}
		_bodies_to_update.clear();

		{
			profiler::scope p2(u8"Update AABBs");

			// update each body, recording removed and added overlaps
			std::vector<body_data_pair> add_contacts;
			std::vector<body_data_pair> remove_contacts;
			for (const _body_aabb_update &cur : bodies_to_update) {
				{
					profiler::scope p3(u8"Dual Query");
					_body_bvh.query_dual_aab(
						cur.target->aabb, cur.new_aabb,
						[&](const body_bvh::leaf_node *other) {
							if (cur.target != other->value) {
								remove_contacts.emplace_back(cur.target, other->value);
							}
						}, [&](const body_bvh::leaf_node *other) {
							if (cur.target != other->value) {
								add_contacts.emplace_back(cur.target, other->value);
							}
						}, [](const body_bvh::leaf_node*) {
							// do nothing if the overlap is still there
						}
					);
				}

				{
					profiler::scope p3(u8"Update");
					cur.target->set_aabb(cur.new_aabb, _timestamp);
					if constexpr (use_bvh_updates) {
						_body_bvh.update(cur.target->node, cur.new_aabb);
					} else {
						_body_bvh.detach(cur.target->node);
						_body_bvh.insert(cur.target->node, cur.new_aabb);
					}
					_maybe_validate_bvh();
				}
			}

			// process removed and added overlaps
			std::ranges::sort(add_contacts);
			std::ranges::sort(remove_contacts);
			auto add_it = add_contacts.begin();
			auto remove_it = remove_contacts.begin();
			while (add_it != add_contacts.end() || remove_it != remove_contacts.end()) {
				// disregard an overlap if it's in both sets - its status hasn't changed
				while (add_it != add_contacts.end() && remove_it != remove_contacts.end() && *add_it == *remove_it) {
					++add_it;
					++remove_it;
				}
				if (add_it == add_contacts.end() && remove_it == remove_contacts.end()) {
					break;
				}
				// choose whether to process add or remove event
				if (remove_it == remove_contacts.end() || (add_it != add_contacts.end() && *add_it < *remove_it)) {
					// process add event
					if (!is_collision_disabled(add_it->first, add_it->second)) {
						const auto [it, inserted] = _overlaps.emplace(*add_it, overlap_data());
						crash_if(!inserted);
					}
					++add_it;
				} else {
					const bool found = _overlaps.erase(*remove_it) != 0;
					if (!found) {
						crash_if(!is_collision_disabled(remove_it->first, remove_it->second));
					}
					++remove_it;
				}
			}
		}

		{ // insert new bodies
			profiler::scope p2(u8"Insert Bodies");

			for (const _body_aabb_update &add : bodies_to_add) {
				add.target->aabb = add.new_aabb;
				_body_bvh.query_aab(add.new_aabb, [&](const body_bvh::leaf_node *other) {
					if (is_collision_disabled(add.target, other->value)) {
						return;
					}
					const auto [it, inserted] =
						_overlaps.emplace(body_data_pair(add.target, other->value), overlap_data());
					crash_if(!inserted);
				});
				_body_bvh.insert(add.target->node, add.new_aabb);
			}
		}

		{ // finally, update all existing contacts
			profiler::scope p2(u8"Detect Collisions");

			for (auto it = _overlaps.begin(); it != _overlaps.end(); ++it) {
				it->second.update_contact(it->first.first->this_body, it->first.second->this_body);
			}
		}
	}

	void world::on_body_moved(body_data *bdata) {
		const aab3s tight_aab =
			bdata->this_body.body_shape->get_aabb_with_transform(bdata->this_body.state.position);
		if (!bdata->aabb.contains(tight_aab)) {
			_bodies_to_update.emplace_back(
				bdata, _get_expanded_aab(tight_aab, bdata->this_body.state.velocity.linear)
			);
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
