#pragma once

/// \file
/// Spring constraints.

#include "pbd/common.h"
#include "pbd/math/matrix.h"
#include "pbd/math/vector.h"
#include "pbd/body.h"

namespace pbd::constraints {
	/// Properties of a spring constraint.
	struct spring_constraint_properties {
		/// No initialization.
		spring_constraint_properties(uninitialized_t) {
		}

		double length; ///< The length of this spring.
		double inverse_stiffness; ///< The inverse stiffness of this spring.
	};

	/// A constraint between two particles that follows the Hooke's law.
	struct particle_spring {
		/// No initialization.
		particle_spring(uninitialized_t) {
		}

		/// Projects this constraint.
		void project(cvec3d &x1, cvec3d &x2, double inv_m1, double inv_m2, double inv_dt2, double &lambda) const {
			cvec3d t = x2 - x1;
			double t_len = t.norm();
			double t_diff = t_len - properties.length;
			double c = t_diff;
			double inv_k_dt2 = properties.inverse_stiffness * inv_dt2;
			double delta_lambda = -(c + inv_k_dt2 * lambda) / (inv_m1 + inv_m2 + inv_k_dt2);
			lambda += delta_lambda;
			cvec3d dx = (delta_lambda / t_len) * t;
			x1 -= inv_m1 * dx;
			x2 += inv_m2 * dx;
		}

		spring_constraint_properties properties = uninitialized; ///< Properties of this constraint.
		std::size_t particle1; ///< The first particle affected by this constraint.
		std::size_t particle2; ///< The second particle affected by this constraint.
	};

	/// A constraint between two bodies that follows the Hooke's law.
	struct body_spring {
		/// No initialization.
		body_spring(uninitialized_t) {
		}

		// TODO

		spring_constraint_properties properties = uninitialized; ///< Properties of this constraint.
		/// Offset of the spring's connection to \ref body1 in its local coordinates.
		cvec3d offset1 = uninitialized;
		/// Offset of the spring's connection to \ref body2 in its local coordinates.
		cvec3d offset2 = uninitialized;
		body *body1; ///< The first body.
		body *body2; ///< The second body.
	};
}
