#pragma once

/// \file
/// The separation axis test.

#include "lotus/collision/shapes/convex_polyhedron.h"
#include "lotus/physics/body_properties.h"

namespace lotus::collision {
	/// The sseparation axis test.
	struct sat_t {
		/// The result of the algorithm.
		struct result {
			/// No initialization.
			result(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			result(vec3 a, scalar d) : axis(a), distance(d) {
			}

			/// The axis along which the two polyhedron have the smallest overlap. This may be negated to ensure that
			/// \ref polyhedron2 is further along the axis than \ref polyhedron1.
			vec3 axis = uninitialized;
			scalar distance; ///< Separation distance. Possibly negative, which indicates penetration.
		};

		/// Finds the separation axis between the two polyhedron.
		[[nodiscard]] static result operator()(polyhedron_pair);
	};
	constexpr sat_t sat; ///< Global instance for convenience.
}
