#pragma once

/// \file
/// Common collision related definitions.

#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"
#include "lotus/physics/common.h"

namespace lotus::collision {
	using namespace lotus::physics::types;
	using namespace lotus::physics::constants;

	namespace shapes {
		struct polyhedron;
	}


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

	/// A pair of polyhedra, represented by their shapes and positions.
	struct polyhedron_pair {
		/// The result of projecting both polyhedron along a specific axis.
		struct axis_projection_result {
			/// No initialization.
			axis_projection_result(uninitialized_t) {
			}
			/// Initializes this object to an initial state for the algorithm.
			axis_projection_result(std::nullopt_t) :
				distance(-std::numeric_limits<scalar>::infinity()), shape2_after_shape2(false) {
			}
			/// Initializes all fields of this struct.
			axis_projection_result(scalar d, bool s2_after_s1) : distance(d), shape2_after_shape2(s2_after_s1) {
			}

			scalar distance; ///< Separation distance. Possibly negative, which indicates penetration.
			bool shape2_after_shape2; ///< Whether \ref polyhedron2 is further along the axis than \ref polyhedron1.
		};

		/// No initialization.
		polyhedron_pair(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		constexpr polyhedron_pair(
			const shapes::polyhedron &s1,
			physics::body_position p1,
			const shapes::polyhedron &s2,
			physics::body_position p2
		) : position1(p1), position2(p2), shape1(&s1), shape2(&s2) {
		}

		physics::body_position position1 = uninitialized; ///< Position of \ref shape1.
		physics::body_position position2 = uninitialized; ///< Position of \ref shape2.
		const shapes::polyhedron *shape1; ///< The first shape.
		const shapes::polyhedron *shape2; ///< The second shape.

		/// Returns the support vertex for the given direction.
		[[nodiscard]] simplex_vertex support_vertex(vec3) const;
		/// Returns the position, in global coordinates, of the given \ref simplex_vertex.
		[[nodiscard]] vec3 simplex_vertex_position(simplex_vertex) const;
		/// Returns the penetration along the given axis.
		[[nodiscard]] axis_projection_result penetration_distance(vec3) const;
	};
}
