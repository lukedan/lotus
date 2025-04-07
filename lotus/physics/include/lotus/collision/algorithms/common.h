#pragma once

/// \file
/// Common data types for collision detection algorithms.

#include "lotus/common.h"

namespace lotus::collision {
	/// A vertex in a simplex.
	struct simplex_vertex {
		/// No initialization.
		simplex_vertex(uninitialized_t) {
		}
		/// Creates a vertex from the given indices.
		simplex_vertex(u32 i1, u32 i2) : index1(i1), index2(i2) {
		}

		/// Equality.
		friend bool operator==(simplex_vertex, simplex_vertex) = default;

		u32 index1; ///< Vertex index in the first polyhedron.
		u32 index2; ///< Vertex index in the second polyhedron.
	};
}
