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
	/// The position of a rigid body.
	struct body_position {
	public:
		/// No initialization.
		body_position(uninitialized_t) {
		}
		/// Initializes the position with the given position and orientation.
		[[nodiscard]] constexpr static body_position at(vec3 x, uquats q) {
			return body_position(x, q);
		}

		/// Converts the given local space position to world space.
		[[nodiscard]] constexpr vec3 local_to_global(vec3 local) const {
			return position + orientation.rotate(local);
		}
		/// Converts the given world space position to local space.
		[[nodiscard]] constexpr vec3 global_to_local(vec3 global) const {
			return orientation.inverse().rotate(global - position);
		}

		vec3 position = uninitialized; ///< The position of the center of mass in world space.
		uquats orientation = uninitialized; ///< The rotation/orientation of this body.
	private:
		/// Initializes all fields of this struct.
		constexpr body_position(vec3 x, uquats q) : position(x), orientation(q) {
		}
	};
	/// The velocity of a rigid body; first order time derivative of a \ref body_position.
	struct body_velocity {
	public:
		/// No initialization.
		body_velocity(uninitialized_t) {
		}
		/// Initializes velocity to zero.
		constexpr body_velocity(zero_t) : body_velocity(zero, zero) {
		}
		/// Initializes all fields of this struct.
		[[nodiscard]] constexpr static body_velocity from_linear_angular(vec3 l, vec3 a) {
			return body_velocity(l, a);
		}

		vec3 linear = uninitialized; ///< Linear velocity.
		vec3 angular = uninitialized; ///< Angular velocity.
	private:
		/// Initializes all fields of this struct.
		constexpr body_velocity(vec3 l, vec3 a) : linear(l), angular(a) {
		}
	};
	/// Position and velocity information about a body.
	struct body_state {
	public:
		/// No initialization.
		body_state(uninitialized_t) {
		}
		/// Initializes the body state with the given position, orientation, and linear and angular velocities.
		[[nodiscard]] constexpr static body_state from_position_velocity(body_position pos, body_velocity vel) {
			return body_state(pos, vel);
		}
		/// Initializes the body state to be stationary at the given position and orientation.
		[[nodiscard]] constexpr static body_state stationary_at(vec3 x, uquats q) {
			return body_state(body_position::at(x, q), zero);
		}

		body_position position = uninitialized;
		body_velocity velocity = uninitialized;
	protected:
		/// Initializes all fields of this struct.
		constexpr body_state(body_position p, body_velocity v) : position(p), velocity(v) {
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
	public:
		/// No initialization.
		particle_state(uninitialized_t) {
		}
		/// Initializes the particle state with the given position and velocity.
		[[nodiscard]] constexpr static particle_state at(vec3 pos, vec3 vel) {
			return particle_state(pos, vel);
		}
		/// Initializes the particle to be stationary with the given position.
		[[nodiscard]] constexpr static particle_state stationary_at(vec3 pos) {
			return particle_state(pos, zero);
		}

		vec3 position = uninitialized; ///< The position of this particle.
		vec3 velocity = uninitialized; ///< The velocity of this particle.
	protected:
		/// Initializes all fields of this struct.
		constexpr particle_state(vec3 x, vec3 v) : position(x), velocity(v) {
		}
	};
}
