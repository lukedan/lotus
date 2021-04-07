#pragma once

/// \file
/// A finite-element face.

#include "pbd/common.h"
#include "pbd/math/matrix.h"
#include "pbd/math/vector.h"

namespace pbd::constraints {
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
				double lambda, double shear_modulus
			) {
				constraint_properties result = uninitialized;
				result.inverse_stiffness = zero;
				for (std::size_t i = 0; i < 3; ++i) {
					result.inverse_stiffness(i, i) = lambda + 2.0 * shear_modulus;
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
				double young_modulus, double poisson_ratio
			) {
				double lambda =
					young_modulus * poisson_ratio /
					((young_modulus + poisson_ratio) * (young_modulus - 2.0 * poisson_ratio));
				double shear_modulus = 0.5 * young_modulus / (1.0 + poisson_ratio);
				return from_lame_parameters(lambda, shear_modulus);
			}

			matrix<6, 6, double> inverse_stiffness = uninitialized; ///< Inverse stiffness matrix.
		};
		/// The state of this constraint.
		struct constraint_state {
			/// No initialization.
			constraint_state(uninitialized_t) {
			}

			/// Initializes the state from the rest pose.
			[[nodiscard]] inline static constraint_state from_rest_pose(
				cvec3d p1, cvec3d p2, cvec3d p3, double thickness
			) {
				constraint_state result = uninitialized;

				result.prev_delta_lambda = zero;

				cvec3d d1 = p2 - p1;
				cvec3d d2 = p3 - p1;
				cvec3d normal = vec::cross(d1, d2);
				double area2 = normal.norm();
				normal /= area2;
				mat33d configuration = matd::concat_columns(d1, d2, normal);
				result.inverse_configuration = configuration.inverse();
				result.thickness = thickness;
				result.area = 0.5 * area2;
				
				return result;
			}

			/// Inverse configuration matrix of this face, used for deformation gradient computation.
			mat33d inverse_configuration = uninitialized;
			matrix<6, 1, double> prev_delta_lambda = uninitialized; ///< Lambda deltas of the previous projection step.
			double thickness; ///< Sheet thickness.
			double area; ///< Undeformed surface area.
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
			cvec3d &p1, cvec3d &p2, cvec3d &p3,
			double inv_m1, double inv_m2, double inv_m3,
			double inv_dt2, column_vector<6, double> &lambda,
			projection_type proj_type = projection_type::gauss_seidel
		) {
			cvec3d d1 = p2 - p1;
			cvec3d d2 = p3 - p1;
			double d1_len = d1.norm();
			cvec3d d1_norm = d1 / d1_len;

			cvec3d normal = vec::cross(d1, d2);
			double normal_len = normal.norm();
			cvec3d normal_norm = normal / normal_len;
			double sqrt_vol = std::sqrt(state.area * state.thickness);

			mat33d r_t = // rotation matrix from surface to world space
				matd::concat_columns(d1_norm, vec::cross(normal_norm, d1_norm), normal_norm);
			mat33d r = r_t.transposed();
			mat33d f = // deformation gradient
				matd::concat_columns(r * d1, r * d2, cvec3d(0.0, 0.0, 1.0)) *
				state.inverse_configuration;
			mat33d g = 0.5 * (f.transposed() * f - mat33d::identity());

			column_vector<6, double> c(g(0, 0), g(1, 1), g(2, 2), g(0, 1), g(0, 2), g(1, 2));
			c *= sqrt_vol;

			mat33d df_dx = matd::concat_rows(
				-(state.inverse_configuration.row(0) + state.inverse_configuration.row(1)),
				state.inverse_configuration.row(0),
				state.inverse_configuration.row(1)
			).transposed();
			mat33d f2_t = (f * sqrt_vol).transposed();
			mat33d f2_t_half = 0.5 * f2_t;
			matrix<6, 9, double> dep_dx = uninitialized;
			dep_dx.set_block(0, 0, matd::kronecker_product(df_dx.row(0), f2_t.row(0)));
			dep_dx.set_block(1, 0, matd::kronecker_product(df_dx.row(1), f2_t.row(1)));
			dep_dx.set_block(2, 0, matd::kronecker_product(df_dx.row(2), f2_t.row(2)));
			dep_dx.set_block(
				3, 0,
				matd::kronecker_product(df_dx.row(0), f2_t_half.row(1)) +
				matd::kronecker_product(df_dx.row(1), f2_t_half.row(0))
			);
			dep_dx.set_block(
				4, 0,
				matd::kronecker_product(df_dx.row(0), f2_t_half.row(2)) +
				matd::kronecker_product(df_dx.row(2), f2_t_half.row(0))
			);
			dep_dx.set_block(
				5, 0,
				matd::kronecker_product(df_dx.row(1), f2_t_half.row(2)) +
				matd::kronecker_product(df_dx.row(2), f2_t_half.row(1))
			);
			matrix<9, 6, double> dep_dx_t_over_m = dep_dx.transposed();
			for (std::size_t y = 0; y < 3; ++y) {
				for (std::size_t x = 0; x < 6; ++x) {
					dep_dx_t_over_m(y, x) *= inv_m1;
					dep_dx_t_over_m(y + 3, x) *= inv_m2;
					dep_dx_t_over_m(y + 6, x) *= inv_m3;
				}
			}

			auto lhs =
				matd::multiply_into_symmetric(dep_dx, dep_dx_t_over_m) +
				properties.inverse_stiffness * inv_dt2;
			auto rhs = -(c + properties.inverse_stiffness * (lambda * inv_dt2));
			matrix<6, 1, double> delta_lambda = uninitialized;
			if (proj_type == projection_type::exact) {
				delta_lambda = matd::lup_decompose(lhs).solve(rhs);
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
