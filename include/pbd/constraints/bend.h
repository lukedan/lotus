#pragma once

/// \file
/// Edge bending constraint.

#include "pbd/common.h"
#include "pbd/math/vector.h"

namespace pbd::constraints {
	/// Bending constraint between two triangles that share a single edge.
	struct bend {
		/// Properties of a bending constraint.
		struct constraint_properties {
		public:
			/// No initialization.
			constraint_properties(uninitialized_t) {
			}

			/// Computes the inverse stiffness from the material properties.
			[[nodiscard]] inline static constraint_properties from_material_properties(
				double young_modulus, double poisson_ratio, double thickness
			) {
				return constraint_properties(12.0 * (1 - poisson_ratio * poisson_ratio) / (young_modulus * thickness * thickness));
			}

			double inverse_stiffness; ///< The inverse stiffness of this constraint.
		protected:
			/// Initializes all fields of this object.
			explicit constraint_properties(double inv_k) : inverse_stiffness(inv_k) {
			}
		};
		/// The state of this constraint.
		struct constraint_state {
		public:
			/// No initialization.
			constraint_state(uninitialized_t) {
			}

			/// Initializes the state from the rest pose and surface thickness.
			[[nodiscard]] inline static constraint_state from_rest_pose(
				const cvec3d &e1, const cvec3d &e2, const cvec3d &x3, const cvec3d &x4
			) {
				cvec3d d1 = e2 - e1;
				cvec3d d2 = x3 - e1;
				cvec3d d3 = x4 - e1;
				double d1_norm = d1.norm();
				cvec3d d1n = d1 / d1_norm;

				cvec3d n1 = vec::cross(d1, d2);
				cvec3d n2 = vec::cross(d1, d3);
				double inv_n1_norm = 1.0 / n1.norm();
				double inv_n2_norm = 1.0 / n2.norm();
				n1 *= inv_n1_norm;
				n2 *= inv_n2_norm;

				double cosv = vec::dot(n1, n2);
				cvec3d sin_vec = vec::cross(n1, n2);
				double sinv = vec::dot(sin_vec, d1n);
				double theta = std::atan2(sinv, cosv);

				return constraint_state(inv_n1_norm, inv_n2_norm, theta, d1_norm);
			}

			/// The square root of the sum of inverse areas of the two triangle faces.
			double sqrt_sum_inverse_areas;
			double rest_angle; ///< The angle between the two faces 
			double edge_length; ///< The length of this edge.
		protected:
			/// Initializes all fields of this struct.
			constraint_state(double inv_a1, double inv_a2, double angle, double edge_len) :
				sqrt_sum_inverse_areas(std::sqrt(inv_a1 + inv_a2)),
				rest_angle(angle), edge_length(edge_len) {
			}
		};

		/// No initialization.
		bend(uninitialized_t) {
		}

		/// Clamps the angle to between -pi and pi.
		[[nodiscard]] constexpr static double clamp_angle(double theta) {
			if (theta < -pi) {
				return theta + 2.0 * pi;
			}
			if (theta > pi) {
				return theta - 2.0 * pi;
			}
			return theta;
		}

		/// Projects this constraint.
		void project(
			cvec3d &x1, cvec3d &x2, cvec3d &x3, cvec3d &x4,
			double inv_m1, double inv_m2, double inv_m3, double inv_m4,
			double inv_dt2, double &lambda
		) {
			cvec3d d1 = x2 - x1;
			cvec3d d2 = x3 - x1;
			cvec3d d3 = x4 - x1;
			double inv_d1_norm = 1.0 / d1.norm();
			cvec3d d1n = d1 * inv_d1_norm;

			cvec3d n1 = vec::cross(d1, d2);
			cvec3d n2 = vec::cross(d1, d3);
			double inv_n1_norm = 1.0 / n1.norm();
			double inv_n2_norm = 1.0 / n2.norm();
			n1 *= inv_n1_norm;
			n2 *= inv_n2_norm;

			double cosv = vec::dot(n1, n2);
			cvec3d sin_vec = vec::cross(n1, n2);
			double sinv = vec::dot(sin_vec, d1n);
			double theta = std::atan2(sinv, cosv);

			mat33d i_minus_d1nd1nt_over_d1norm = inv_d1_norm * (mat33d::identity() - d1n * d1n.transposed());
			mat33d i_minus_n1n1t_over_n1norm = inv_n1_norm * (mat33d::identity() - n1 * n1.transposed());
			mat33d i_minus_n2n2t_over_n2norm = inv_n2_norm * (mat33d::identity() - n2 * n2.transposed());
			mat33d n2_cross_i_minus_n1n1t_over_n1norm = vec::cross_product_matrix(n2) * i_minus_n1n1t_over_n1norm;
			mat33d n1_cross_i_minus_n2n2t_over_n2norm = vec::cross_product_matrix(n1) * i_minus_n2n2t_over_n2norm;

			auto dtheta_dx2 =
				cosv * (
					sin_vec.transposed() * i_minus_d1nd1nt_over_d1norm +
					d1n.transposed() * (
						n2_cross_i_minus_n1n1t_over_n1norm * vec::cross_product_matrix(d2) -
						n1_cross_i_minus_n2n2t_over_n2norm * vec::cross_product_matrix(d3)
					)
				) + sinv * (
					n1.transposed() * i_minus_n2n2t_over_n2norm * vec::cross_product_matrix(d3) +
					n2.transposed() * i_minus_n1n1t_over_n1norm * vec::cross_product_matrix(d2)
				);
			auto dtheta_dx3 =
				-cosv * (d1n.transposed() * (n2_cross_i_minus_n1n1t_over_n1norm * vec::cross_product_matrix(d1))) -
				sinv * (n2.transposed() * i_minus_n1n1t_over_n1norm * vec::cross_product_matrix(d1));
			auto dtheta_dx4 =
				cosv * (d1n.transposed() * n1_cross_i_minus_n2n2t_over_n2norm * vec::cross_product_matrix(d1)) -
				sinv * (n1.transposed() * i_minus_n2n2t_over_n2norm * vec::cross_product_matrix(d1));
			auto dtheta_dx1 = -dtheta_dx2 - dtheta_dx3 - dtheta_dx4;

			double c_coefficient = state.sqrt_sum_inverse_areas * state.edge_length / std::sqrt(8.0);

			double theta_diff = clamp_angle(theta - state.rest_angle);
			double c = c_coefficient * theta_diff;

			double alpha_hat = properties.inverse_stiffness * inv_dt2;
			double delta_lambda = -(c + alpha_hat * lambda) / (
				(c_coefficient * inv_m1) * dtheta_dx1.squared_norm() +
				(c_coefficient * inv_m2) * dtheta_dx2.squared_norm() +
				(c_coefficient * inv_m3) * dtheta_dx3.squared_norm() +
				(c_coefficient * inv_m4) * dtheta_dx4.squared_norm() +
				alpha_hat
			);
			lambda += delta_lambda;
			x1 += (c_coefficient * delta_lambda * inv_m1) * dtheta_dx1.transposed();
			x2 += (c_coefficient * delta_lambda * inv_m2) * dtheta_dx2.transposed();
			x3 += (c_coefficient * delta_lambda * inv_m3) * dtheta_dx3.transposed();
			x4 += (c_coefficient * delta_lambda * inv_m4) * dtheta_dx4.transposed();
		}

		constraint_properties properties = uninitialized; ///< The properties of this constraint.
		constraint_state state = uninitialized; ///< The state of this constraint.
		std::size_t particle_edge1; ///< Index of the first particle on the shared edge.
		std::size_t particle_edge2; ///< Index of the second particle on the shared edge.
		std::size_t particle3; ///< Index of the third particle. This particle is not on the shared edge.
		std::size_t particle4; ///< Index of the fourth particle. This particle is not on the shared edge.
	};
}
