#pragma once

/// \file
/// Common physics engine definitions.

#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"
#include "lotus/collision/common.h"

namespace lotus::physics {
	using namespace lotus::collision::types;
	using namespace lotus::collision::constants;

	using body_position = collision::body_position;
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
