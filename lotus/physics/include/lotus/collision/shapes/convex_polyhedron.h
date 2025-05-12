#pragma once

/// \file
/// Polyhedrons.

#include <vector>

#include "lotus/math/vector.h"
#include "lotus/algorithms/convex_hull.h"
#include "lotus/collision/common.h"
#include "lotus/physics/body_properties.h"

namespace lotus::collision::shapes {
	/// A convex polyhedron. The polyhedron is placed so that the center of mass of the polyhedron is at the origin
	/// of its local coordinates.
	struct convex_polyhedron {
		/// Additional properties of a polyhedron. Used to compute its inertia matrix and center the polyhedron.
		struct properties {
			/// No initialization.
			properties(uninitialized_t) {
			}
			/// Initializes all fields to zero.
			properties(zero_t) : covariance_matrix(zero), weighted_vertices(zero), sum_determinants(0.0f) {
			}

			/// Adds a face to this struct.
			void add_face(vec3, vec3, vec3);

			/// The covariance matrix, computed with respect to the origin.
			mat33s covariance_matrix = uninitialized;
			vec3 weighted_vertices = uninitialized; ///< The sum of all vertices weighted by the determinants.
			scalar sum_determinants; ///< The sum of all determinants computed using faces.

			/// Returns the volume of the polyhedron.
			[[nodiscard]] float get_volume() const {
				return sum_determinants / 6.0f;
			}
			/// Returns the center of mass of the polyhedron.
			[[nodiscard]] vec3 get_center_of_mass() const {
				return weighted_vertices / (sum_determinants * 4.0f);
			}
			/// Returns the covariance matrix of this polyhedron translated by the given offset.
			[[nodiscard]] mat33s get_translated_covariance_matrix(vec3 dx) const {
				const vec3 center_of_mass = get_center_of_mass();
				return covariance_matrix + get_volume() * (
					dx * center_of_mass.transposed() +
					center_of_mass * dx.transposed() +
					mat::multiply_into_symmetric(dx, dx.transposed())
				);
			}
			/// Returns the \ref body_properties corresponding to this polyhedron with the given density, with its
			/// center of mass placed at the origin.
			[[nodiscard]] physics::body_properties get_body_properties(scalar density) const {
				const mat33s c = density * get_translated_covariance_matrix(-get_center_of_mass());
				return physics::body_properties::create(mat33s::identity() * c.trace() - c, get_volume() * density);
			}
		};
		/// A arbitrary polygonal face of a polyhedron.
		struct face {
			/// Vertex indices of the face in clockwise order looking from the outside.
			std::vector<u32> vertex_indices;
		};
		/// The result of projecting a polyhedron onto an axis.
		struct axis_projection {
			/// No initialization.
			axis_projection(uninitialized_t) {
			}
			/// Initializes this object to the appropriate initial state for finding the projection.
			axis_projection(std::nullopt_t) :
				min_vertex(std::numeric_limits<u32>::max()),
				max_vertex(std::numeric_limits<u32>::max()),
				min(std::numeric_limits<scalar>::max()),
				max(-std::numeric_limits<scalar>::max()) {
			}

			u32 min_vertex; ///< Index of the vertex that has the smallest dot product with a given axis.
			u32 max_vertex; ///< Index of the vertex that has the largest dot product with a given axis.
			scalar min; ///< The dot product between \ref min_vertex and the given axis.
			scalar max; ///< The dot product between \ref max_vertex and the given axis.
		};

		/// Minimum dot product between two normals for them to be considered similar.
		constexpr static scalar unique_normal_threshold = 0.999f;
		/// Minimum dot product between two edges for them to be considered similar.
		constexpr static scalar unique_edge_threshold = 0.999f;

		// TODO allocator or pool?
		std::vector<vec3> vertices; ///< Vertices of this polyhedron.
		std::vector<vec3> unique_face_normals; ///< List of unique face normals.
		std::vector<vec3> unique_edge_directions; ///< List of unique edge directions.
		std::vector<face> faces; ///< All faces of the polyhedron.

		/// Processes the given list of vertices and creates a polyhedron from its convex hull, computing its rigid
		/// body properties in the process.
		[[nodiscard]] static std::pair<convex_polyhedron, properties> bake(std::span<const vec3> verts);

		/// Returns the index of the support vertex in the given direction, and its dot product with the direction.
		[[nodiscard]] std::pair<u32, scalar> get_support_vertex(vec3 dir) const;
		/// Returns the range that this polyhedron covers when projected onto the given axis.
		[[nodiscard]] axis_projection project_onto_axis(vec3) const;
		/// Returns the range that this polyhedron covers when projected onto the given axis, assuming that it has
		/// the given transform.
		[[nodiscard]] axis_projection project_onto_axis_with_transform(vec3, body_position) const;
	};
}
