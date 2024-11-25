#pragma once

/// \file
/// Edge bending constraint.

#include "lotus/common.h"
#include "lotus/math/vector.h"

namespace lotus::physics::constraints {
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
				scalar young_modulus, scalar poisson_ratio, scalar thickness
			) {
				return constraint_properties(
					12.0f * (1.0f - poisson_ratio * poisson_ratio) / (young_modulus * thickness * thickness)
				);
			}

			scalar inverse_stiffness; ///< The inverse stiffness of this constraint.
		protected:
			/// Initializes all fields of this object.
			explicit constraint_properties(scalar inv_k) : inverse_stiffness(inv_k) {
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
				const vec3 &e1, const vec3 &e2, const vec3 &x3, const vec3 &x4
			) {
				const vec3 d1 = e2 - e1;
				const vec3 d2 = x3 - e1;
				const vec3 d3 = x4 - e1;
				const scalar d1_norm = d1.norm();
				const vec3 d1n = d1 / d1_norm;

				vec3 n1 = vec::cross(d1, d2);
				vec3 n2 = vec::cross(d1, d3);
				const scalar inv_n1_norm = 1.0f / n1.norm();
				const scalar inv_n2_norm = 1.0f / n2.norm();
				n1 *= inv_n1_norm;
				n2 *= inv_n2_norm;

				const scalar cosv = vec::dot(n1, n2);
				const vec3 sin_vec = vec::cross(n1, n2);
				const scalar sinv = vec::dot(sin_vec, d1n);
				const scalar theta = std::atan2(sinv, cosv);

				return constraint_state(inv_n1_norm, inv_n2_norm, theta, d1_norm);
			}

			/// The square root of the sum of inverse areas of the two triangle faces.
			scalar sqrt_sum_inverse_areas;
			scalar rest_angle; ///< The angle between the two faces 
			scalar edge_length; ///< The length of this edge.
		protected:
			/// Initializes all fields of this struct.
			constraint_state(scalar inv_a1, scalar inv_a2, scalar angle, scalar edge_len) :
				sqrt_sum_inverse_areas(std::sqrt(inv_a1 + inv_a2)),
				rest_angle(angle), edge_length(edge_len) {
			}
		};

		/// No initialization.
		bend(uninitialized_t) {
		}

		/// Clamps the angle to between -pi and pi.
		[[nodiscard]] constexpr static scalar clamp_angle(scalar theta) {
			if (theta < -pi) {
				return theta + 2.0f * pi;
			}
			if (theta > pi) {
				return theta - 2.0f * pi;
			}
			return theta;
		}

		/// Projects this constraint.
		void project(
			vec3 &x1, vec3 &x2, vec3 &x3, vec3 &x4,
			scalar inv_m1, scalar inv_m2, scalar inv_m3, scalar inv_m4,
			scalar inv_dt2, scalar &lambda
		) {
			const vec3 d1 = x2 - x1;
			const vec3 d2 = x3 - x1;
			const vec3 d3 = x4 - x1;
			const scalar inv_d1_norm = 1.0f / d1.norm();
			const vec3 d1n = d1 * inv_d1_norm;

			vec3 n1 = vec::cross(d1, d2);
			vec3 n2 = vec::cross(d1, d3);
			const scalar inv_n1_norm = 1.0f / n1.norm();
			const scalar inv_n2_norm = 1.0f / n2.norm();
			n1 *= inv_n1_norm;
			n2 *= inv_n2_norm;

			const scalar cosv = vec::dot(n1, n2);
			const vec3 sin_vec = vec::cross(n1, n2);
			const scalar sinv = vec::dot(sin_vec, d1n);
			const scalar theta = std::atan2(sinv, cosv);

			const mat33s i_minus_d1nd1nt_over_d1norm = inv_d1_norm * (mat33s::identity() - d1n * d1n.transposed());
			const mat33s i_minus_n1n1t_over_n1norm = inv_n1_norm * (mat33s::identity() - n1 * n1.transposed());
			const mat33s i_minus_n2n2t_over_n2norm = inv_n2_norm * (mat33s::identity() - n2 * n2.transposed());
			const mat33s n2_cross_i_minus_n1n1t_over_n1norm =
				vec::cross_product_matrix(n2) * i_minus_n1n1t_over_n1norm;
			const mat33s n1_cross_i_minus_n2n2t_over_n2norm =
				vec::cross_product_matrix(n1) * i_minus_n2n2t_over_n2norm;

			const auto dtheta_dx2 =
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
			const auto dtheta_dx3 =
				-cosv * (d1n.transposed() * (n2_cross_i_minus_n1n1t_over_n1norm * vec::cross_product_matrix(d1))) -
				sinv * (n2.transposed() * i_minus_n1n1t_over_n1norm * vec::cross_product_matrix(d1));
			const auto dtheta_dx4 =
				cosv * (d1n.transposed() * n1_cross_i_minus_n2n2t_over_n2norm * vec::cross_product_matrix(d1)) -
				sinv * (n1.transposed() * i_minus_n2n2t_over_n2norm * vec::cross_product_matrix(d1));
			const auto dtheta_dx1 = -dtheta_dx2 - dtheta_dx3 - dtheta_dx4;

			const scalar c_coefficient = state.sqrt_sum_inverse_areas * state.edge_length / std::sqrt(8.0f);

			const scalar theta_diff = clamp_angle(theta - state.rest_angle);
			const scalar c = c_coefficient * theta_diff;

			const scalar alpha_hat = properties.inverse_stiffness * inv_dt2;
			const scalar delta_lambda = -(c + alpha_hat * lambda) / (
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
