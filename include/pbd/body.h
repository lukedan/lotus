#pragma once

/// \file
/// Properties of rigid bodies.

#include "math/matrix.h"
#include "math/vector.h"
#include "math/quaternion.h"

namespace pbd {
	/// Properties that are inherent to a rigid body.
	struct body_properties {
		/// No initialization.
		body_properties(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr body_properties(double m, cvec3d_t m_center, mat33d i) :
			inertia(i), center_of_mass(m_center), inverse_mass(1.0 / m) {
		}

		mat33d inertia = uninitialized; ///< The inertia matrix.
		cvec3d_t center_of_mass = uninitialized; ///< Center of mass.
		double inverse_mass; ///< Inverse mass.
	};
	/// Position and velocity information about a body.
	struct body_state {
		/// No initialization.
		body_state(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr body_state(cvec3d_t x, uquatd_t q, cvec3d_t v, cvec3d_t omega) :
			position(x), rotation(q), linear_velocity(v), angular_velocity(omega) {
		}

		cvec3d_t position = uninitialized; ///< The position of the center of mass in world space.
		uquatd_t rotation = uninitialized; ///< The rotation/orientation of this body.
		cvec3d_t linear_velocity = uninitialized; ///< Linear velocity of the center of mass.
		cvec3d_t angular_velocity = uninitialized; ///< Angular velocity around the center of mass.
	};

	/// Properties that are inherent to a particle.
	struct particle_properties {
	public:
		/// No initialization.
		particle_properties(uninitialized_t) {
		}

		/// Creates a new \ref particle_properties object using the given inverse mass.
		[[nodiscard]] constexpr static particle_properties from_mass(double m) {
			return particle_properties(1.0 / m);
		}
		/// Creates a new \ref particle_properties with infinite mass, indicating that it is not affected by external
		/// forces.
		[[nodiscard]] constexpr static particle_properties kinematic() {
			return particle_properties(0.0);
		}

		double inverse_mass; ///< Inverse mass.
	protected:
		/// Initializes all fields of this struct.
		constexpr explicit particle_properties(double inv_m) : inverse_mass(inv_m) {
		}
	};
	/// Position and velocity information about a particle.
	struct particle_state {
		/// No initialization.
		particle_state(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr particle_state(cvec3d_t x, cvec3d_t v) : position(x), velocity(v) {
		}

		cvec3d_t position = uninitialized; ///< The position of this particle.
		cvec3d_t velocity = uninitialized; ///< The velocity of this particle.
	};
}
