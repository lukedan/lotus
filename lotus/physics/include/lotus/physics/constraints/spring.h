#pragma once

/// \file
/// Spring constraints.

#include "lotus/common.h"
#include "lotus/math/matrix.h"
#include "lotus/math/vector.h"
#include "lotus/physics/body.h"

namespace lotus::physics::constraints {
	/// Properties of a spring constraint.
	struct spring_constraint_properties {
		/// No initialization.
		spring_constraint_properties(uninitialized_t) {
		}

		scalar length; ///< The length of this spring.
		scalar inverse_stiffness; ///< The inverse stiffness of this spring.
	};

	/// A constraint between two particles that follows the Hooke's law.
	struct particle_spring {
		/// No initialization.
		particle_spring(uninitialized_t) {
		}

		/// Projects this constraint.
		void project(vec3 &x1, vec3 &x2, scalar inv_m1, scalar inv_m2, scalar inv_dt2, scalar &lambda) const {
			vec3 t = x2 - x1;
			scalar t_len = t.norm();
			scalar t_diff = t_len - properties.length;
			scalar c = t_diff;
			scalar inv_k_dt2 = properties.inverse_stiffness * inv_dt2;
			scalar delta_lambda = -(c + inv_k_dt2 * lambda) / (inv_m1 + inv_m2 + inv_k_dt2);
			lambda += delta_lambda;
			vec3 dx = (delta_lambda / t_len) * t;
			x1 -= inv_m1 * dx;
			x2 += inv_m2 * dx;
		}

		spring_constraint_properties properties = uninitialized; ///< Properties of this constraint.
		usize particle1; ///< The first particle affected by this constraint.
		usize particle2; ///< The second particle affected by this constraint.
	};

	/// A constraint between two bodies that follows the Hooke's law.
	struct body_spring {
		/// No initialization.
		body_spring(uninitialized_t) {
		}

		// TODO

		spring_constraint_properties properties = uninitialized; ///< Properties of this constraint.
		/// Offset of the spring's connection to \ref body1 in its local coordinates.
		vec3 offset1 = uninitialized;
		/// Offset of the spring's connection to \ref body2 in its local coordinates.
		vec3 offset2 = uninitialized;
		body *body1; ///< The first body.
		body *body2; ///< The second body.
	};
}
