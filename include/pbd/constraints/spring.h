#pragma once

/// \file
/// Spring constraints.

#include "../common.h"

namespace pbd::constraints {
	/// A constraint that follows the Hooke's law.
	struct spring {
		/// Properties of a spring constraint.
		struct constraint_properties {
			/// No initialization.
			constraint_properties(uninitialized_t) {
			}

			double length; ///< The length of this spring.
			double inverse_stiffness; ///< The inverse stiffness of this spring.
		};

		/// No initialization.
		spring(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		spring(constraint_properties prop, std::size_t o1, std::size_t o2) :
			properties(prop), object1(o1), object2(o2) {
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

		constraint_properties properties = uninitialized; ///< Properties of this constraint.
		std::size_t object1; ///< The first object affected by this constraint.
		std::size_t object2; ///< The second object affected by this constraint.
	};
}
