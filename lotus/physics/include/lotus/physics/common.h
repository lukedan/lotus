#pragma once

/// \file
/// Common physics engine definitions.

#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"
#include "lotus/collision/common.h"

namespace lotus::physics {
	using namespace lotus::collision::types;
	using namespace lotus::collision::constants;

	/// Epsilon values used in various places.
	namespace epsilons {
		/// Minimum squared length of contact tangent when computed from relative velocity.
		constexpr scalar contact_tangent = 1e-8f;
	}

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
		/// Initializes an object representing the given linear velocity and zero angular velocity.
		[[nodiscard]] constexpr static body_velocity from_linear(vec3 l) {
			return body_velocity(l, zero);
		}
		/// Initializes all fields of this struct.
		[[nodiscard]] constexpr static body_velocity from_linear_angular(vec3 l, vec3 a) {
			return body_velocity(l, a);
		}

		vec3 linear = uninitialized; ///< Linear velocity.
		vec3 angular = uninitialized; ///< Angular velocity.

		/// Returns the linar and angular components concatenated into a vector.
		[[nodiscard]] column_vector<6, scalar> get_vector() const {
			return column_vector<6, scalar>(linear, angular);
		}

		/// Returns the global linear velocity at the given offset.
		[[nodiscard]] vec3 get_velocity_at(vec3 local_offset) const {
			return linear + vec::cross(angular, local_offset);
		}
	private:
		/// Initializes all fields of this struct.
		constexpr body_velocity(vec3 l, vec3 a) : linear(l), angular(a) {
		}
	};
	/// Position and velocity information about a body.
	struct body_state {
		/// No initialization.
		body_state(uninitialized_t) {
		}
		/// Initializes the body state to be stationary at the given position and orientation.
		[[nodiscard]] constexpr static body_state stationary_at(body_position bp) {
			return body_state(bp, zero);
		}
		/// \overload
		[[nodiscard]] constexpr static body_state stationary_at(vec3 x, uquats q) {
			return body_state(body_position(x, q), zero);
		}
		/// Initializes the body state with the given position and velocity, leaving its angular momentum at zero.
		[[nodiscard]] constexpr static body_state from_position_velocity(body_position bp, vec3 v) {
			return body_state(bp, body_velocity::from_linear(v));
		}
		/// \overload
		[[nodiscard]] constexpr static body_state from_position_velocity(vec3 x, uquats q, vec3 v) {
			return body_state(body_position(x, q), body_velocity::from_linear(v));
		}

		body_position position = uninitialized; ///< The position and orientation of the body.
		body_velocity velocity = uninitialized; ///< The linear and angular velocity of the body.
	protected:
		/// Initializes all fields of this struct.
		constexpr body_state(body_position p, body_velocity v) : position(p), velocity(v) {
		}
	};
}
