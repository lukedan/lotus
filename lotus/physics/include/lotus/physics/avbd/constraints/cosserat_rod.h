#pragma once

/// \file
/// Cosserat rod related constraints.

#include "lotus/physics/common.h"

namespace lotus::physics::avbd::constraints::cosserat_rod {
	/// The basis vector chosen as the local direction of the rod.
	constexpr static vec3 direction_basis = { 0.0f, 0.0f, 1.0f };

	/// The cosserat rod bending-twisting constraint.
	struct bend_twist {
		// No initialization.
		bend_twist(uninitialized_t) {
		}

		u32 orientation1; ///< Index of the first orientation.
		u32 orientation2; ///< Index of the second orientation.
		scalar stiffness; ///< Stiffness.
		uquats initial_bend = uninitialized; ///< Initial bending between the two orientations.
	};

	/// The cosserat rod stretching-shearing constraint.
	struct stretch_shear {
		/// No initialization.
		stretch_shear(uninitialized_t) {
		}

		u32 particle1; ///< Index of the first particle.
		u32 particle2; ///< Index of the second particle.
		u32 orientation; ///< Index of the orientation between the two particles.
		scalar stiffness; ///< Stiffness.
		scalar initial_length; ///< Initial distance between the two particles.
	};
}
