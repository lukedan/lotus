#pragma once

/// \file
/// Common physics engine definitions.

#include "lotus/math/constants.h"
#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"

namespace lotus::physics {
	/// Type definitions for the physics engine.
	inline namespace types {
		using scalar = float; ///< Scalar type.
		using vec3 = cvec3<scalar>; ///< Vector type.
		using quats = quaternion<scalar>; ///< Quaternion type.
		using uquats = unit_quaternion<scalar>; ///< Unit quaternion type.
		using mat33s = mat33<scalar>; ///< 3x3 matrix type.
	}

	inline namespace constants {
		constexpr static scalar pi = static_cast<scalar>(lotus::constants::pi); ///< Pi.
	}

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
}
