#pragma once

/// \file
/// Properties of rigid bodies and particles.

#include "math/matrix.h"
#include "math/vector.h"
#include "math/quaternion.h"

namespace pbd {
	/// Properties of a rigid body material.
	struct material_properties {
		/// No initialization.
		material_properties(uninitialized_t) {
		}
		/// Creates a new material properties from the given parameters.
		[[nodiscard]] inline static material_properties create(double static_fric, double dyn_fric, double rest) {
			material_properties result = uninitialized;
			result.static_friction = static_fric;
			result.dynamic_friction = dyn_fric;
			result.restitution = rest;
			return result;
		}

		double static_friction; ///< Static friction coefficient.
		double dynamic_friction; ///< Dynamic friction coefficient.
		double restitution; ///< Restitution coefficient.
	};

	/// Properties that are inherent to a rigid body.
	struct body_properties {
	public:
		/// No initialization.
		body_properties(uninitialized_t) {
		}

		/// Initializes a body with the given inertia matrix and mass.
		[[nodiscard]] static constexpr body_properties create(mat33d i, double m) {
			return body_properties(i.inverse(), 1.0 / m);
		}
		/// Initializes a body with infinity mass, which is not affected by external forces or torques.
		[[nodiscard]] static constexpr body_properties kinematic() {
			return body_properties(zero, 0.0);
		}

		mat33d inverse_inertia = uninitialized; ///< Inverse of the inertia matrix.
		double inverse_mass; ///< Inverse mass.
	protected:
		/// Initializes all fields of this struct.
		constexpr body_properties(mat33d inv_i, double inv_m) :
			inverse_inertia(inv_i), inverse_mass(inv_m) {
		}
	};
	/// Position and velocity information about a body.
	struct body_state {
	public:
		/// No initialization.
		body_state(uninitialized_t) {
		}
		/// Initializes the body state with the given position, orientation, and linear and angular velocities.
		[[nodiscard]] constexpr static body_state at(cvec3d pos, uquatd rot, cvec3d lin_vel, cvec3d ang_vel) {
			return body_state(pos, rot, lin_vel, ang_vel);
		}
		/// Initializes the body to be stationary, with the given position and orientation.
		[[nodiscard]] constexpr static body_state stationary_at(cvec3d pos, uquatd rot) {
			return body_state(pos, rot, zero, zero);
		}

		cvec3d position = uninitialized; ///< The position of the center of mass in world space.
		uquatd rotation = uninitialized; ///< The rotation/orientation of this body.
		cvec3d linear_velocity = uninitialized; ///< Linear velocity of the center of mass.
		cvec3d angular_velocity = uninitialized; ///< Angular velocity around the center of mass.
	protected:
		/// Initializes all fields of this struct.
		constexpr body_state(cvec3d x, uquatd q, cvec3d v, cvec3d omega) :
			position(x), rotation(q), linear_velocity(v), angular_velocity(omega) {
		}
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
	public:
		/// No initialization.
		particle_state(uninitialized_t) {
		}
		/// Initializes the particle state with the given position and velocity.
		[[nodiscard]] constexpr static particle_state at(cvec3d pos, cvec3d vel) {
			return particle_state(pos, vel);
		}
		/// Initializes the particle to be stationary with the given position.
		[[nodiscard]] constexpr static particle_state stationary_at(cvec3d pos) {
			return particle_state(pos, zero);
		}

		cvec3d position = uninitialized; ///< The position of this particle.
		cvec3d velocity = uninitialized; ///< The velocity of this particle.
	protected:
		/// Initializes all fields of this struct.
		constexpr particle_state(cvec3d x, cvec3d v) : position(x), velocity(v) {
		}
	};
}
