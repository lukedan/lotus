#pragma once

/// \file
/// The expanding polytope algorithm.

#include "lotus/collision/common.h"
#include "lotus/collision/algorithms/gjk.h"

/// The expanding polytope algorithm.
namespace lotus::collision::epa {
	/// Results from the algorithm.
	struct result {
		/// The type of this contact.
		enum class type {
			vertex_face, ///< A vertex on the first polyhedron collides with a face on the second polyhedron.
			face_vertex, ///< A face on the firxt polyhedron collides with a vertex on the second polyhedron.
			edge_edge,   ///< Two edges on both polyhedra collides with each other.
		};

		/// No initialization.
		result(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		result(std::array<vec3, 3> positions, std::array<simplex_vertex, 3> verts, vec3 n, scalar d) :
			simplex_positions(positions), vertices(verts), normal(n), penetration_depth(d) {
		}

		/// Positions of \ref vertices.
		std::array<vec3, 3> simplex_positions{ uninitialized, uninitialized, uninitialized };
		/// Vertices of the contact plane.
		std::array<simplex_vertex, 3> vertices{ uninitialized, uninitialized, uninitialized };
		/// Normalized contact normal. This points out of the first body and into the second body.
		vec3 normal = uninitialized;
		scalar penetration_depth; ///< Penetration depth.

		/// Analyzes the contact vertices and returns the type of this result.
		[[nodiscard]] type compute_type() const;
	};

	/// Executes the algorithm.
	[[nodiscard]] result epa(polyhedron_pair, gjk::result);
}
