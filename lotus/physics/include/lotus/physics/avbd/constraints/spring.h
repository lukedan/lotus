#pragma once

/// \file
/// Spring constraints for AVBD.

#include "lotus/physics/body.h"

namespace lotus::physics::avbd::constraints {
	/// A spring constraint.
	struct spring {
		body *body1 = nullptr; ///< The first body.
		body *body2 = nullptr; ///< The second body.
		vec3 local_position1 = zero; ///< Local position on the first body.
		vec3 local_position2 = zero; ///< Local position on the second body.
		scalar initial_length = 0.0f; ///< Initial length of this constraint.
		scalar compressed_stiffness = 0.0f; ///< Stiffness when this spring is compressed.
		scalar stretched_stiffness = 0.0f; ///< Stiffness when this spring is stretched.

		/// Returns \ref local_position1 translated to global coordinates.
		[[nodiscard]] vec3 get_global_position1() const {
			return body1 ? body1->state.position.local_to_global(local_position1) : local_position1;
		}
		/// Returns \ref local_position2 translated to global coordinates.
		[[nodiscard]] vec3 get_global_position2() const {
			return body2 ? body2->state.position.local_to_global(local_position2) : local_position2;
		}
	};
}
