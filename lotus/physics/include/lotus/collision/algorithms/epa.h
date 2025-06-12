#pragma once

/// \file
/// The expanding polytope algorithm.

#include "lotus/collision/common.h"
#include "lotus/collision/algorithms/gjk.h"

/// The expanding polytope algorithm.
namespace lotus::collision::epa {
	/// Results from the algorithm.
	struct result {
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
		vec3 normal = uninitialized; ///< Contact normal.
		scalar penetration_depth; ///< Penetration depth.
	};

	/// Executes the algorithm.
	[[nodiscard]] result epa(polyhedron_pair, gjk::result);
}
