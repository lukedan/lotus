#pragma once

/// \file
/// Contact constraints for AVBD.

#include "lotus/physics/body.h"

namespace lotus::physics::avbd::constraints {
	/// Rigid body contact constraint.
	struct rigid_body_contact {
		/// No initialization.
		rigid_body_contact(uninitialized_t) {
		}

		body *body1; ///< The first body involved.
		body *body2; ///< The second body involved.
		/// Contact tangent frame in world space. The normal points out of the first body and into the second body.
		tangent_frame<scalar> tangents = uninitialized;
		vec3 local_pos1 = uninitialized; ///< Local position of the contact point on \ref body1.
		vec3 local_pos2 = uninitialized; ///< Local position of the contact point on \ref body2.

		vec3 stiffness = uninitialized; ///< Soft stiffness.
		vec3 lambda = uninitialized; ///< The dual variable used to emulate a stiff constraint.

		/// Returns \ref local_pos1 transformed from the local space of \ref body1 to the global space.
		[[nodiscard]] constexpr vec3 get_global_pos1() const {
			return body1->state.position.local_to_global(local_pos1);
		}
		/// Returns \ref local_pos2 transformed from the local space of \ref body2 to the global space.
		[[nodiscard]] constexpr vec3 get_global_pos2() const {
			return body2->state.position.local_to_global(local_pos2);
		}
	};
}
