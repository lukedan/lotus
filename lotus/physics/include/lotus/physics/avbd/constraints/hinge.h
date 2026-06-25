#pragma once

/// \file
/// Hinge constraints.

#include "lotus/physics/body.h"

namespace lotus::physics::avbd::constraints {
	/// A hinge constraint.
	struct hinge {
		/// Zero initialization.
		hinge(zero_t) {
		}

		body *body1 = nullptr; ///< The first body.
		body *body2 = nullptr; ///< The second body.
		vec3 local_axis1 = zero; ///< Normalized rotation axis that will be aligned with \ref local_axis2.
		vec3 local_axis2 = zero; ///< Normalized rotation axis that will be aligned with \ref local_axis1.
		bool disable_collision = false; ///< Disables collision between \ref body1 and \ref body2.

		/// Returns \ref local_axis1 transformed into the global coordinate space.
		[[nodiscard]] vec3 get_global_axis1() const {
			return body1 ? body1->state.position.orientation.rotate(local_axis1) : local_axis1;
		}
		/// Returns \ref local_axis2 transformed into the global coordinate space.
		[[nodiscard]] vec3 get_global_axis2() const {
			return body2 ? body2->state.position.orientation.rotate(local_axis2) : local_axis2;
		}
	};
}
