#pragma once

/// \file
/// The Gilbert-Johnson-Kerrthi algorithm.

#include <array>

#include "lotus/collision/common.h"

/// Implementation of the Gilbert-Johnson-Keerthi algorithm.
namespace lotus::collision::gjk {
	/// Result of the GJK algorithm that does not persist between time steps.
	struct transient_result {
		/// No initialization.
		transient_result(uninitialized_t) {
		}

		/// Vertex positions of the simplex.
		std::array<vec3, 4> simplex_positions{
			uninitialized, uninitialized, uninitialized, uninitialized
		};
		/// Indicates whether the normals of faces at even indices need to be inverted. This is only valid when a
		/// simplex has been successfully created by the GJK algorithm, i.e., when \ref operator() returns
		/// \p true (there may be other cases where this is valid but this is usually not relevant in those
		/// cases).
		bool invert_even_normals;
	};
	/// Result of the GJK algorithm that can be reused between time steps.
	struct persistent_result {
		/// No initialization.
		persistent_result(uninitialized_t) {
		}
		/// Initializes the result to the initial state for the algorithm.
		persistent_result(zero_t) : simplex_vertices(0) {
		}

		/// Vertices of the simplex.
		std::array<simplex_vertex, 4> simplex{ uninitialized, uninitialized, uninitialized, uninitialized };
		usize simplex_vertices; ///< The number of valid vertices in \ref simplex.
	};
	/// Result of the algorithm.
	struct result {
	public:
		/// No initialization.
		result(uninitialized_t) {
		}
		/// Returns an object that indicates intersection.
		[[nodiscard]] static result intersects(persistent_result p, transient_result t) {
			return result(p, t, true);
		}
		/// Returns an object that indicates no intersection.
		[[nodiscard]] static result does_not_intersect(persistent_result p, transient_result t) {
			return result(p, t, false);
		}

		persistent_result persistent = uninitialized; ///< Persistent part of the result.
		transient_result transient = uninitialized; ///< Transient part of the result.
		bool has_intersection; ///< Whether there's an intersection.
	private:
		/// Initializes all fields of this struct.
		result(persistent_result p, transient_result t, bool hi) :
			persistent(p), transient(t), has_intersection(hi) {
		}
	};

	/// Updates and returns the result of the GJK algorithm.
	[[nodiscard]] result gjk(polyhedron_pair, persistent_result = zero);
}
