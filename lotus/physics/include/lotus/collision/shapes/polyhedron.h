#pragma once

/// \file
/// Polyhedrons.

#include <vector>

#include "lotus/math/vector.h"
#include "lotus/algorithms/convex_hull.h"
#include "lotus/collision/common.h"
#include "lotus/physics/body_properties.h"

namespace lotus::collision::shapes {
	/// A convex polyhedron defined by a series of vertices.
	struct polyhedron {
		/// Inertia matrix, center of mass, and volume of a polyhedron.
		struct properties {
			/// No initialization.
			properties(uninitialized_t) {
			}

			/// Computes the polyhedron properties for the given set of vertices and triangle faces.
			[[nodiscard]] static properties compute_for(
				std::span<const vec3>, std::span<const std::array<u32, 3>>
			);

			/// The covariance matrix, computed with respect to the origin instead of the center of mass.
			mat33s covariance_matrix = uninitialized;
			vec3 center_of_mass = uninitialized; ///< Center of mass.
			scalar volume; ///< The volume of this object.

			/// Returns the \ref body_properties corresponding to this polyhedron with the given density.
			[[nodiscard]] physics::body_properties get_body_properties(scalar density) const {
				mat33s c = density * covariance_matrix;
				return physics::body_properties::create(mat33s::identity() * c.trace() - c, volume * density);
			}

			/// Returns the covariance matrix of this polyhedron translated by the given offset.
			[[nodiscard]] mat33s translated_covariance_matrix(vec3 dx) const {
				return covariance_matrix + volume * (
					dx * center_of_mass.transposed() +
					center_of_mass * dx.transposed() +
					mat::multiply_into_symmetric(dx, dx.transposed())
				);
			}
			/// Returns the properties of this polyhedron translated by the given offset.
			[[nodiscard]] properties translated(vec3 dx) const {
				properties result = *this;
				result.covariance_matrix = translated_covariance_matrix(dx);
				result.center_of_mass += dx;
				return result;
			}
		};

		std::vector<vec3> vertices; ///< Vertices of this polyhedron.

		/// Offsets this shape so that the center of mass is at the origin, and returns the resulting
		/// \ref body_properties.
		[[nodiscard]] physics::body_properties bake(scalar density);

		/// Returns the index of the support vertex in the given direction, and its dot product with the direction.
		[[nodiscard]] std::pair<u32, scalar> get_support_vertex(vec3 dir) const;
	};
}
