#pragma once

/// \file
/// Cosserat rod constraints for XPBD.

#include "lotus/physics/common.h"
#include "lotus/physics/body.h"

namespace lotus::physics::xpbd::constraints::cosserat_rod {
	/// The basis vector chosen as the local direction of the rod.
	constexpr static vec3 direction_basis = { 0.0f, 0.0f, 1.0f };

	/// Stretching-shearing constraint.
	struct stretch_shear {
		/// Lagrangians for a single constraint.
		using lagrangians = vec3;

		/// No initialization.
		stretch_shear(uninitialized_t) {
		}

		/// Projects this constraint.
		void project(particle &p1, particle &p2, orientation&, scalar inv_dt2, lagrangians&) const;

		u32 particle1; ///< Index of the first particle.
		u32 particle2; ///< Index of the second particle.
		u32 orientation; ///< Index of the orientation between the two particles.
		scalar compliance; ///< Compliance, i.e., inverse stiffness.
		scalar inv_initial_length; ///< Inverse of the initial length of this constraint.
	};

	/// Bending-twisting constraint.
	struct bend_twist {
		/// Lagrangians for a single constraint.
		using lagrangians = vec4;

		/// No initialization.
		bend_twist(uninitialized_t) {
		}

		/// Projects this constraint.
		void project(orientation &o1, orientation &o2, scalar inv_dt2, lagrangians&) const;

		u32 orientation1; ///< Index of the first orientation.
		u32 orientation2; ///< Index of the second orientation.
		scalar compliance; ///< Compliance, i.e., inverse stiffness.
		uquats initial_bend = uninitialized; ///< Initial bending of this constraint.
	};
}
