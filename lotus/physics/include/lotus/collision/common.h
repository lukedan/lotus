#pragma once

/// \file
/// Common collision related definitions.

#include "lotus/math/constants.h"
#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"

namespace lotus::collision {
	/// Type definitions for the physics engine.
	inline namespace types {
		using scalar = float; ///< Scalar type.
		using vec3 = cvec3<scalar>; ///< Vector type.
		using quats = quaternion<scalar>; ///< Quaternion type.
		using uquats = unit_quaternion<scalar>; ///< Unit quaternion type.
		using mat33s = mat33<scalar>; ///< 3x3 matrix type.
	}

	inline namespace constants {
		constexpr static scalar pi = static_cast<scalar>(lotus::constants::pi); ///< Pi.
	}

	namespace shapes {
		struct convex_polyhedron;
	}


	/// The position of a rigid body.
	struct body_position {
	public:
		/// No initialization.
		body_position(uninitialized_t) {
		}
		/// Initializes the position with the given position and orientation.
		[[nodiscard]] constexpr static body_position at(vec3 x, uquats q) {
			return body_position(x, q);
		}

		/// Converts the given local space position to world space.
		[[nodiscard]] constexpr vec3 local_to_global(vec3 local) const {
			return position + orientation.rotate(local);
		}
		/// Converts the given world space position to local space.
		[[nodiscard]] constexpr vec3 global_to_local(vec3 global) const {
			return orientation.inverse().rotate(global - position);
		}

		vec3 position = uninitialized; ///< The center of mass in world space.
		uquats orientation = uninitialized; ///< The rotation/orientation of this body.
	private:
		/// Initializes all fields of this struct.
		constexpr body_position(vec3 x, uquats q) : position(x), orientation(q) {
		}
	};

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
			const shapes::convex_polyhedron &s1,
			body_position p1,
			const shapes::convex_polyhedron &s2,
			body_position p2
		) : position1(p1), position2(p2), shape1(&s1), shape2(&s2) {
		}

		body_position position1 = uninitialized; ///< Position of \ref shape1.
		body_position position2 = uninitialized; ///< Position of \ref shape2.
		const shapes::convex_polyhedron *shape1; ///< The first shape.
		const shapes::convex_polyhedron *shape2; ///< The second shape.

		/// Returns the support vertex for the given direction.
		[[nodiscard]] simplex_vertex support_vertex(vec3) const;
		/// Returns the position, in global coordinates, of the given \ref simplex_vertex.
		[[nodiscard]] vec3 simplex_vertex_position(simplex_vertex) const;
		/// Returns the penetration along the given axis.
		[[nodiscard]] axis_projection_result penetration_distance(vec3) const;
	};
}
