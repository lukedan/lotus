#pragma once

/// \file
/// Implementation of the physics world.

#include <set>
#include <unordered_map>
#include <unordered_set>

#include "lotus/collision/algorithms/contact_manifold.h"
#include "lotus/collision/algorithms/aabb_tree.h"
#include "lotus/physics/body.h"
#include "lotus/physics/constraints/hinge.h"
#include "lotus/physics/constraints/spring.h"
#include "lotus/physics/constraints/contact.h"
#include "lotus/physics/constraints/pin.h"

namespace lotus::physics {
	/// A physics world that contains bodies that interact.
	class world {
	public:
		constexpr static bool validate_bvh = false; ///< Whether to validate the BVH after each operation.

		/// A pair of bodies. Comparison ignores the order of the two.
		struct body_pair {
			/// Initializes this object to empty.
			body_pair() = default;
			/// Initializes all fields of this struct.
			body_pair(body *b1, body *b2) : body1(b1), body2(b2) {
			}

			body *body1 = nullptr; ///< The first body.
			body *body2 = nullptr; ///< The second body.

			/// Equality comparison. Ignores the order of the two bodies.
			[[nodiscard]] friend bool operator==(const body_pair &lhs, const body_pair &rhs) {
				return
					(lhs.body1 == rhs.body1 && lhs.body2 == rhs.body2) ||
					(lhs.body2 == rhs.body1 && lhs.body1 == rhs.body2);
			}
		};
		/// Hashing for a pair of bodies.
		struct body_pair_hash {
			/// The hashing function.
			[[nodiscard]] constexpr usize operator()(const body_pair &bs) const {
				std::hash<body*> ptr_hasher;
				usize h1 = ptr_hasher(bs.body1);
				usize h2 = ptr_hasher(bs.body2);
				if (h1 > h2) {
					std::swap(h1, h2);
				}
				return hash_combine(h1, h2);
			}
		};
		/// Data associated with two bodies with overlapping AABBs.
		struct overlap_data {
			std::optional<constraints::rigid_body_contact> contact; ///< Contact constraint.

			/// Updates overlap data given the two bodies.
			void update_contact(body*, body*);
			/// \overload
			void update_contact(body_pair bodies) {
				update_contact(bodies.body1, bodies.body2);
			}
		};
		using timestamp_t = u64; ///< Timestamp type.
		/// Map of bodies with overlapping AABBs, and associated contacts if any.
		using overlap_map = std::unordered_map<body_pair, overlap_data, body_pair_hash>;
		using body_bvh = collision::aabb_tree<body*>; ///< BVH containing bodies.
		/// Data associated with a body.
		struct body_data {
			[[no_unique_address]] static_optional<timestamp_t, true> aabb_timestamp; ///< Timestamp of when this AABB was last updated.
			aab3s aabb = zero; ///< The AABB of this body.
			body_bvh::leaf_node *node = nullptr; ///< Node in the AABB tree.
			std::set<body*> overlaps; ///< Other bodies with overlapping AABB nodes.

			/// Sets the AABB alongside with \ref aabb_timestamp.
			void set_aabb(aab3s bb, timestamp_t timestamp) {
				aabb = bb;
				aabb_timestamp = timestamp;
			}
		};


		/// Adds a body to this world.
		void add_body(body &b) {
			const aab3s aabb =
				_get_expanded_aab(b.body_shape->get_aabb_with_transform(b.state.position), b.state.velocity.linear);
			body_bvh::leaf_node *node = _body_bvh.create_node(&b);
			auto [it, inserted] = _body_map.emplace(&b, body_data());
			crash_if(!inserted);
			it->second.node = node;
			it->second.set_aabb(aabb, _timestamp);
			_bodies_to_update.emplace(&b);
		}
		/// Calls the given callback for each body.
		template <typename Cb> void for_each_body(Cb &&cb) {
			for (const auto &[b, bd] : _body_map) {
				cb(b);
			}
		}
		/// \overload
		template <typename Cb> void for_each_body(Cb &&cb) const {
			for (const auto &[b, bd] : _body_map) {
				cb(static_cast<const body*>(b));
			}
		}

		/// Calls the given callback for each contact constraint.
		template <typename Cb> void for_each_contact(Cb &&cb) const {
			for (const auto &[k, v] : _overlap) {
				if (v.contact) {
					cb(v.contact.value());
				}
			}
		}

		/// Detects collisions and updates \ref contacts.
		void update_contact_constraints();

		/// Marks the body for an AABB update if necessary.
		void on_body_moved(body*);

		/// Returns the overlap map.
		[[nodiscard]] const overlap_map &get_overlap_map() const {
			return _overlap;
		}
		/// Returns the body BVH.
		[[nodiscard]] const body_bvh &get_body_bvh() const {
			return _body_bvh;
		}
		/// Returns the data associated with the given body..
		[[nodiscard]] const body_data &get_body_data(const body *b) const {
			return _body_map.at(const_cast<body*>(b)); // TODO evil!
		}
		/// Returns the world timestamp.
		[[nodiscard]] timestamp_t get_timestamp() const {
			return _timestamp;
		}
		/// Returns the number of bodies in this world.
		[[nodiscard]] usize get_num_bodies() const {
			return _body_map.size();
		}

		vec3 gravity = zero; ///< Gravity.
		/// Enlarges all objects by this threshold for preventing flickering contacts.
		scalar collision_threshold = 0.001f;
		/// How much the AABB of a body is expanded in the direction of its velocity.
		scalar aabb_prediction = 1.0f / 30.0f;
		/// Amount to expand AABBs by.
		scalar aabb_expansion = 0.01f;

		std::vector<constraints::spring> springs; ///< All spring constraints.
		std::vector<constraints::pin> pins; ///< All pin constraints.
		std::vector<constraints::hinge> hinges; ///< All hinge constraints.
	private:
		timestamp_t _timestamp = 0; ///< Timestamp incremented each time \ref update_contact_constraints() is called.
		body_bvh _body_bvh; ///< Bodies in this world.
		std::unordered_map<body*, body_data> _body_map; ///< Body data.
		std::unordered_set<body*> _bodies_to_update; ///< Bodies that have invalid overlap data.
		overlap_map _overlap; ///< All contacts in the current time step.

		/// Validates the BVH if enabled.
		void _maybe_validate_bvh() const;

		/// Expands the given AABB.
		[[nodiscard]] aab3s _get_expanded_aab(aab3s aab, vec3 velocity) const {
			const vec3 offset1 = velocity * aabb_prediction;
			aab3s new_aab = aab;
			new_aab.min = matm::min(new_aab.min, new_aab.min + offset1);
			new_aab.max = matm::max(new_aab.max, new_aab.max + offset1);
			new_aab.min -= vec3::filled(aabb_expansion);
			new_aab.max += vec3::filled(aabb_expansion);
			return new_aab;
		}
	};
}
