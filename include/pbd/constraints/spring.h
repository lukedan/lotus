#pragma once

/// \file
/// Spring constraints.

#include "../common.h"

namespace pbd::constraints {
	/// A constraint that follows the Hooke's law.
	struct spring {
	public:
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
		void project(cvec3d_t &x1, cvec3d_t &x2, double inv_m1, double inv_m2, double inv_dt2, double &lambda) const {
			cvec3d_t t = x2 - x1;
			double t_norm = t.norm();
			double t_diff = t_norm - properties.length;
			double c_over_k = 0.5 * t_diff * t_diff;
			double inv_k = properties.inverse_stiffness;
			double delta_lambda_times_k =
				-(c_over_k + inv_k * inv_k * inv_dt2 * lambda) /
				((inv_m1 + inv_m2) * t_diff * t_diff + inv_k * inv_k * inv_k * inv_dt2);
			lambda += delta_lambda_times_k * inv_k;
			x1 -= inv_m1 * t_diff * delta_lambda_times_k * (t / t_norm);
			x2 += inv_m2 * t_diff * delta_lambda_times_k * (t / t_norm);
		}

		constraint_properties properties = uninitialized; ///< Properties of this constraint.
		std::size_t object1; ///< The first object affected by this constraint.
		std::size_t object2; ///< The second object affected by this constraint.
	};
}
