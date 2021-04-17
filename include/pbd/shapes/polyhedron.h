#pragma once

/// \file
/// Polyhedrons.

#include <vector>

#include "pbd/math/vector.h"

namespace pbd::shapes {
	/// A polyhedron defined by a series of vertices.
	struct polyhedron {
		std::vector<cvec3d> vertices; ///< Vertices of this polyhedron.
	};
}
