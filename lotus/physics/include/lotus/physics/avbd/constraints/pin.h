#pragma once

/// \file
/// Pinning constraints for AVBD.

namespace lotus::physics::avbd::constraints {
	/// Constraint that fixes two points on the two bodies at the same place. Also known as ball and socket joint.
	struct pin {
		/// Zero initialization.
		pin(zero_t) {
		}

		body *body1 = nullptr; ///< The first body.
		body *body2 = nullptr; ///< The second body.
		vec3 local_position1 = zero; ///< Position on the first body.
		vec3 local_position2 = zero; ///< Position on the second body.

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
