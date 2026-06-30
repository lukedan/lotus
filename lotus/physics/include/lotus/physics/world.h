#pragma once

/// \file
/// Implementation of the physics world.

#include <span>

#include "lotus/collision/algorithms/contact_manifold.h"
#include "lotus/physics/body.h"
#include "lotus/physics/constraints/hinge.h"
#include "lotus/physics/constraints/spring.h"
#include "lotus/physics/constraints/contact.h"
#include "lotus/physics/constraints/pin.h"

namespace lotus::physics {
	/// A physics world that contains bodies that interact.
	class world {
	public:
		/// Result of collision detection.
		struct rigid_body_collision {
			/// No initialization.
			rigid_body_collision(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			rigid_body_collision(body &b1, body &b2, collision::contact_manifold manifold) :
				body1(&b1), body2(&b2), contact_manifold(std::move(manifold)) {
			}

			body *body1; ///< The first body.
			body *body2; ///< The second body.
			collision::contact_manifold contact_manifold; ///< The contact manifold.
		};

		/// Computes a contact tangent frame so that the tangent is aligned with the relative velocity at the contact
		/// point.
		[[nodiscard]] static tangent_frame<scalar> select_tangent_frame_for_contact(
			const body&, const body&, vec3 local_pos1, vec3 local_pos2, vec3 contact_normal
		);

		/// Detects collisions between all rigid bodies in this world.
		[[nodiscard]] std::vector<rigid_body_collision> detect_collisions() const;
		/// Detects collisions and updates \ref contacts.
		void update_contact_constraints();

		/// Adds a body to this world.
		void add_body(body &b) {
			_bodies.emplace_back(&b);
		}
		/// Returns the list of bodies.
		[[nodiscard]] std::span<body *const> get_bodies() const {
			return _bodies;
		}

		vec3 gravity = zero; ///< Gravity.
		/// Enlarges all objects by this threshold for preventing flickering contacts.
		scalar collision_threshold = 0.001f;

		std::vector<constraints::rigid_body_contact> contacts; ///< All contacts in the current time step.
		std::vector<constraints::spring> springs; ///< All spring constraints.
		std::vector<constraints::pin> pins; ///< All pin constraints.
		std::vector<constraints::hinge> hinges; ///< All hinge constraints.
	private:
		std::vector<body*> _bodies; ///< The list of bodies in this world.
	};
}
