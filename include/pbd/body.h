#pragma once

/// \file
/// Properties of rigid bodies.

#include "pbd/math/matrix.h"
#include "pbd/math/vector.h"
#include "pbd/math/quaternion.h"

namespace pbd {
	/// Properties that are inherent to a rigid body.
	struct body_properties {
	public:
		/// No initialization.
		body_properties(uninitialized_t) {
		}

		/// Initializes a body with the given inertia matrix, center of mass, and mass.
		[[nodiscard]] static constexpr body_properties create(mat33d i, cvec3d c, double m) {
			return body_properties(1.0 / m, c, i);
		}
		/// Initializes a body with infinity mass, which is not affected by external forces.
		[[nodiscard]] static constexpr body_properties kinematic() {
			return body_properties(0.0, zero, mat33d::identity());
		}

		mat33d inertia = uninitialized; ///< The inertia matrix.
		cvec3d center_of_mass = uninitialized; ///< Center of mass.
		double inverse_mass; ///< Inverse mass.
	protected:
		/// Initializes all fields of this struct.
		constexpr body_properties(double inv_m, cvec3d m_center, mat33d i) :
			inertia(i), center_of_mass(m_center), inverse_mass(inv_m) {
		}
	};
	/// Position and velocity information about a body.
	struct body_state {
		/// No initialization.
		body_state(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr body_state(cvec3d x, uquatd q, cvec3d v, cvec3d omega) :
			position(x), rotation(q), linear_velocity(v), angular_velocity(omega) {
		}

		cvec3d position = uninitialized; ///< The position of the center of mass in world space.
		uquatd rotation = uninitialized; ///< The rotation/orientation of this body.
		cvec3d linear_velocity = uninitialized; ///< Linear velocity of the center of mass.
		cvec3d angular_velocity = uninitialized; ///< Angular velocity around the center of mass.
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
		constexpr particle_state(cvec3d x, cvec3d v) : position(x), velocity(v) {
		}

		cvec3d position = uninitialized; ///< The position of this particle.
		cvec3d velocity = uninitialized; ///< The velocity of this particle.
	};
}
