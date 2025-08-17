#pragma once

/// \file
/// Properties of rigid bodies and particles.

#include "lotus/math/matrix.h"
#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"
#include "lotus/physics/common.h"

namespace lotus::physics {
	/// Properties of a rigid body material.
	struct material_properties {
		/// No initialization.
		material_properties(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		material_properties(scalar static_fric, scalar dyn_fric, scalar rest) :
			static_friction(static_fric), dynamic_friction(dyn_fric), restitution(rest) {
		}

		scalar static_friction; ///< Static friction coefficient.
		scalar dynamic_friction; ///< Dynamic friction coefficient.
		scalar restitution; ///< Restitution coefficient.
	};

	/// Properties that are inherent to a rigid body.
	struct body_properties {
	public:
		/// No initialization.
		body_properties(uninitialized_t) {
		}
		/// Initializes a body with the given inertia matrix and mass.
		[[nodiscard]] static constexpr body_properties create(mat33s i, scalar m) {
			return body_properties(i.inverse(), 1.0f / m);
		}
		/// Initializes a body with infinity mass, which is not affected by external forces or torques.
		[[nodiscard]] static constexpr body_properties kinematic() {
			return body_properties(zero, zero);
		}

		mat33s inverse_inertia = uninitialized; ///< Inverse of the inertia matrix.
		scalar inverse_mass; ///< Inverse mass.
	protected:
		/// Initializes all fields of this struct.
		constexpr body_properties(mat33s inv_i, scalar inv_m) :
			inverse_inertia(inv_i), inverse_mass(inv_m) {
		}
	};

	/// Properties that are inherent to a particle.
	struct particle_properties {
	public:
		/// No initialization.
		particle_properties(uninitialized_t) {
		}
		/// Creates a new \ref particle_properties object using the given inverse mass.
		[[nodiscard]] constexpr static particle_properties from_mass(scalar m) {
			return particle_properties(1.0f / m);
		}
		/// Creates a new \ref particle_properties with infinite mass, indicating that it is not affected by external
		/// forces.
		[[nodiscard]] constexpr static particle_properties kinematic() {
			return particle_properties(zero);
		}

		scalar inverse_mass; ///< Inverse mass.
	protected:
		/// Initializes all fields of this struct.
		constexpr explicit particle_properties(scalar inv_m) : inverse_mass(inv_m) {
		}
	};
	/// Position and velocity information about a particle.
	struct particle_state {
		/// No initialization.
		particle_state(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr particle_state(vec3 pos, vec3 vel) : position(pos), velocity(vel) {
		}
		/// Initializes the particle to be stationary with the given position.
		[[nodiscard]] constexpr static particle_state stationary_at(vec3 pos) {
			return particle_state(pos, zero);
		}

		vec3 position = uninitialized; ///< The position of this particle.
		vec3 velocity = uninitialized; ///< The velocity of this particle.
	};
	/// Orientation and angular velocity information.
	struct orientation_state {
		/// No initialization.
		orientation_state(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr orientation_state(uquats ori, vec3 angular_vel) : orientation(ori), angular_velocity(angular_vel) {
		}
		/// Initializes the orientation with the given value, and angular velocity to zero.
		[[nodiscard]] constexpr static orientation_state stationary_at(uquats ori) {
			return orientation_state(ori, zero);
		}

		uquats orientation = uninitialized; ///< Orientation.
		vec3 angular_velocity = uninitialized; ///< Angular velocity.
	};
}
