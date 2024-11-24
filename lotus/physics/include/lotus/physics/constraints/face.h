#pragma once

/// \file
/// A finite-element face.

#include "lotus/common.h"
#include "lotus/math/matrix.h"
#include "lotus/math/vector.h"

namespace lotus::physics::constraints {
	/// An elastic triangular face.
	///
	/// \sa Bender et al., Position-Based Simulation of Continuous Materials
	/// \sa Servin et al., Interactive Simulation of Elastic Deformable Materials
	/// \sa Pfaff et al., Adaptive Tearing and Cracking of Thin Sheets
	/// \sa Sifakis, FEM Simulation of 3D Deformable Solids: A practitioner's guide to theory, discretization and model reduction
	struct face {
		/// Properties of this face.
		struct constraint_properties {
			/// No initialization.
			constraint_properties(uninitialized_t) {
			}

			/// Creates the inverse stiffness matrix from the given Lame parameters.
			[[nodiscard]] inline static constraint_properties from_lame_parameters(
				scalar lambda, scalar shear_modulus
			) {
				constraint_properties result = uninitialized;
				result.inverse_stiffness = zero;
				for (std::size_t i = 0; i < 3; ++i) {
					result.inverse_stiffness(i, i) = lambda + 2.0f * shear_modulus;
					result.inverse_stiffness(i + 3, i + 3) = shear_modulus;
				}
				result.inverse_stiffness(0, 1) = result.inverse_stiffness(0, 2) =
					result.inverse_stiffness(1, 0) = result.inverse_stiffness(1, 2) =
					result.inverse_stiffness(2, 0) = result.inverse_stiffness(2, 1) =
					lambda;
				result.inverse_stiffness = result.inverse_stiffness.inverse();
				return result;
			}
			/// Creates the properties from the Young's modulus and Poisson's ratio of the material.
			[[nodiscard]] inline static constraint_properties from_material_properties(
				scalar young_modulus, scalar poisson_ratio
			) {
				scalar lambda =
					young_modulus * poisson_ratio /
					((young_modulus + poisson_ratio) * (young_modulus - 2.0f * poisson_ratio));
				scalar shear_modulus = 0.5f * young_modulus / (1.0f + poisson_ratio);
				return from_lame_parameters(lambda, shear_modulus);
			}

			matrix<6, 6, scalar> inverse_stiffness = uninitialized; ///< Inverse stiffness matrix.
		};
		/// The state of this constraint.
		struct constraint_state {
			/// No initialization.
			constraint_state(uninitialized_t) {
			}

			/// Initializes the state from the rest pose.
			[[nodiscard]] inline static constraint_state from_rest_pose(
				vec3 p1, vec3 p2, vec3 p3, scalar thickness
			) {
				constraint_state result = uninitialized;

				result.prev_delta_lambda = zero;

				vec3 d1 = p2 - p1;
				vec3 d2 = p3 - p1;
				vec3 normal = vec::cross(d1, d2);
				scalar area2 = normal.norm();
				normal /= area2;
				mat33s configuration = mat::concat_columns(d1, d2, normal);
				result.inverse_configuration = configuration.inverse();
				result.thickness = thickness;
				result.area = 0.5f * area2;
				
				return result;
			}

			/// Inverse configuration matrix of this face, used for deformation gradient computation.
			mat33s inverse_configuration = uninitialized;
			matrix<6, 1, scalar> prev_delta_lambda = uninitialized; ///< Lambda deltas of the previous projection step.
			scalar thickness; ///< Sheet thickness.
			scalar area; ///< Undeformed surface area.
		};
		/// Determines how this constraint is projected.
		enum class projection_type {
			exact, ///< It's projected exactly by inverting the matrix.
			gauss_seidel ///< It's projected approximately using one iteration of Gauss-Seidel.
		};

		/// No initialization.
		face(uninitialized_t) {
		}

		/// Projects this constraint.
		void project(
			vec3 &p1, vec3 &p2, vec3 &p3,
			scalar inv_m1, scalar inv_m2, scalar inv_m3,
			scalar inv_dt2, column_vector<6, scalar> &lambda,
			projection_type proj_type = projection_type::gauss_seidel
		) {
			vec3 d1 = p2 - p1;
			vec3 d2 = p3 - p1;
			scalar d1_len = d1.norm();
			vec3 d1_norm = d1 / d1_len;

			vec3 normal = vec::cross(d1, d2);
			scalar normal_len = normal.norm();
			vec3 normal_norm = normal / normal_len;
			scalar sqrt_vol = std::sqrt(state.area * state.thickness);

			mat33s r_t = // rotation matrix from surface to world space
				mat::concat_columns(d1_norm, vec::cross(normal_norm, d1_norm), normal_norm);
			mat33s r = r_t.transposed();
			mat33s f = // deformation gradient
				mat::concat_columns(r * d1, r * d2, vec3(0.0, 0.0, 1.0)) *
				state.inverse_configuration;
			mat33s g = 0.5 * (f.transposed() * f - mat33s::identity());

			column_vector<6, scalar> c(g(0, 0), g(1, 1), g(2, 2), g(0, 1), g(0, 2), g(1, 2));
			c *= sqrt_vol;

			mat33s df_dx = mat::concat_rows(
				-(state.inverse_configuration.row(0) + state.inverse_configuration.row(1)),
				state.inverse_configuration.row(0),
				state.inverse_configuration.row(1)
			).transposed();
			mat33s f2_t = (f * sqrt_vol).transposed();
			mat33s f2_t_half = 0.5 * f2_t;
			matrix<6, 9, scalar> dep_dx = uninitialized;
			dep_dx.set_block(0, 0, mat::kronecker_product(df_dx.row(0), f2_t.row(0)));
			dep_dx.set_block(1, 0, mat::kronecker_product(df_dx.row(1), f2_t.row(1)));
			dep_dx.set_block(2, 0, mat::kronecker_product(df_dx.row(2), f2_t.row(2)));
			dep_dx.set_block(
				3, 0,
				mat::kronecker_product(df_dx.row(0), f2_t_half.row(1)) +
				mat::kronecker_product(df_dx.row(1), f2_t_half.row(0))
			);
			dep_dx.set_block(
				4, 0,
				mat::kronecker_product(df_dx.row(0), f2_t_half.row(2)) +
				mat::kronecker_product(df_dx.row(2), f2_t_half.row(0))
			);
			dep_dx.set_block(
				5, 0,
				mat::kronecker_product(df_dx.row(1), f2_t_half.row(2)) +
				mat::kronecker_product(df_dx.row(2), f2_t_half.row(1))
			);
			matrix<9, 6, scalar> dep_dx_t_over_m = dep_dx.transposed();
			for (std::size_t y = 0; y < 3; ++y) {
				for (std::size_t x = 0; x < 6; ++x) {
					dep_dx_t_over_m(y, x) *= inv_m1;
					dep_dx_t_over_m(y + 3, x) *= inv_m2;
					dep_dx_t_over_m(y + 6, x) *= inv_m3;
				}
			}

			auto lhs =
				mat::multiply_into_symmetric(dep_dx, dep_dx_t_over_m) +
				properties.inverse_stiffness * inv_dt2;
			auto rhs = -(c + properties.inverse_stiffness * (lambda * inv_dt2));
			matrix<6, 1, scalar> delta_lambda = uninitialized;
			if (proj_type == projection_type::exact) {
				delta_lambda = mat::lup_decompose(lhs).solve(rhs);
			} else {
				delta_lambda = state.prev_delta_lambda;
				gauss_seidel::iterate(lhs, rhs, delta_lambda);
				state.prev_delta_lambda = delta_lambda;
			}

			auto delta_x = dep_dx_t_over_m * delta_lambda;
			lambda += delta_lambda;
			p1 += r_t * delta_x.block<3, 1>(0, 0);
			p2 += r_t * delta_x.block<3, 1>(3, 0);
			p3 += r_t * delta_x.block<3, 1>(6, 0);
		}

		constraint_properties properties = uninitialized; ///< The properties of this constraint.
		constraint_state state = uninitialized; ///< The state of this constraint.
		std::size_t particle1; ///< Index of the first particle.
		std::size_t particle2; ///< Index of the second particle.
		std::size_t particle3; ///< Index of the third particle.
	};
}
