#pragma once

/// \file
/// Implementation of the physics world.

#include <set>
#include <unordered_map>

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
		/// If true, the node will be updated in place. If false, the node will be detached and then reinserted into
		/// the BVH. For now, setting this to \p false seems to result in faster lookups.
		constexpr static bool use_bvh_updates = false;

		using timestamp_t = u64; ///< Timestamp type.
		struct body_data;
		using body_bvh = collision::aabb_tree<body_data*>; ///< BVH containing bodies.
		/// Data associated with a body.
		struct body_data {
			/// Initializes \ref this_body;
			explicit body_data(body b) : this_body(std::move(b)) {
			}

			body this_body; ///< The body.

			/// Timestamp of when this AABB was last updated.
			[[no_unique_address]] static_optional<timestamp_t, true> aabb_timestamp;
			aab3s aabb = zero; ///< The AABB of this body.
			body_bvh::leaf_node *node = nullptr; ///< Node in the AABB tree.

			/// Sets the AABB alongside with \ref aabb_timestamp.
			void set_aabb(aab3s bb, timestamp_t timestamp) {
				aabb = bb;
				aabb_timestamp = timestamp;
			}
		};

		using body_data_pair = std::pair<body_data*, body_data*>; ///< A pair of \ref body_data pointers.
		/// Hash function for \ref body_data_pair.
		struct body_pair_hash {
			/// The hash function.
			[[nodiscard]] constexpr static usize operator()(const body_data_pair &p) {
				std::hash<body_data*> ptr_hash;
				return hash_combine(ptr_hash(p.first), ptr_hash(p.second));
			}
		};
		/// Data associated with two bodies with overlapping AABBs.
		struct overlap_data {
			std::optional<constraints::rigid_body_contact> contact; ///< Contact constraint.

			/// Updates overlap data given the two bodies.
			void update_contact(body&, body&);
		};
		/// Map of bodies with overlapping AABBs, and associated contacts if any.
		using overlap_map = std::unordered_map<body_data_pair, overlap_data, body_pair_hash>;


		/// Adds a body to this world.
		body_data *add_body(body raw_body) {
			body_data *bdata = &*_bodies.emplace_back(std::make_unique<body_data>(std::move(raw_body)));
			body *b = &bdata->this_body;

			const aab3s aabb = _get_expanded_aab(
				b->body_shape->get_aabb_with_transform(b->state.position), b->state.velocity.linear
			);

			body_bvh::leaf_node *node = _body_bvh.create_node(bdata);
			bdata->node = node;
			bdata->set_aabb(aabb, _timestamp);

			_bodies_to_update.emplace_back(bdata, aabb);

			return bdata;
		}
		/// Returns the list of bodies.
		[[nodiscard]] std::span<const std::unique_ptr<body_data>> get_bodies() const {
			return _bodies;
		}

		/// Calls the given callback for each contact constraint.
		template <typename Cb> void for_each_contact(Cb &&cb) const {
			for (const auto &[k, v] : _overlaps) {
				if (v.contact) {
					cb(v.contact.value());
				}
			}
		}

		/// Detects collisions and updates \ref contacts.
		void update_contact_constraints();

		/// Marks the body for an AABB update if necessary.
		void on_body_moved(body_data*);

		/// Returns the overlap map.
		[[nodiscard]] const overlap_map &get_overlaps() const {
			return _overlaps;
		}
		/// Returns the body BVH.
		[[nodiscard]] const body_bvh &get_body_bvh() const {
			return _body_bvh;
		}
		/// Returns the world timestamp.
		[[nodiscard]] timestamp_t get_timestamp() const {
			return _timestamp;
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
		/// Information about updating the AABB of a body.
		struct _body_aabb_update {
			/// Zero initialization.
			_body_aabb_update(zero_t) {
			}
			/// Initializes all fields of this struct.
			_body_aabb_update(body_data *t, aab3s new_bb) : target(t), new_aabb(new_bb) {
			}

			body_data *target = nullptr; ///< The body to update.
			aab3s new_aabb = zero; ///< New AABB.
		};

		timestamp_t _timestamp = 0; ///< Timestamp incremented each time \ref update_contact_constraints() is called.
		body_bvh _body_bvh; ///< Bodies in this world.
		std::vector<std::unique_ptr<body_data>> _bodies; ///< All bodies.
		std::vector<_body_aabb_update> _bodies_to_update; ///< Bodies that have invalid overlap data.
		overlap_map _overlaps; ///< All contacts in the current time step.

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
