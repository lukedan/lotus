#pragma once

/// \file
/// Interface of the algorithm for finding the contact manifold between two polyhedra.

#include "lotus/containers/short_vector.h"

#include "lotus/collision/shapes/convex_polyhedron.h"
#include "lotus/collision/algorithms/epa.h"
#include "lotus/collision/contact.h"

namespace lotus::collision {
	/// Algorithm for finding the contact manifold between two polyhedra.
	struct contact_manifold {
		using contact_point_list = short_vector<vec3, 6>; ///< List of contact points.
		/// A contact point.
		struct point {
			// No initialization.
			point(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			point(vec3 p1, vec3 p2) : local_position1(p1), local_position2(p2) {
			}

			vec3 local_position1 = uninitialized; ///< Contact point on the first body in its local space.
			vec3 local_position2 = uninitialized; ///< Contact point on the second body in its local space.
		};

		/// Computes one most penetrating vertex from each polyhedron.
		[[nodiscard]] static std::pair<u32, u32> compute_most_penetrating_vertices(
			const epa::result&,
			const shapes::convex_polyhedron&,
			const body_position&,
			const shapes::convex_polyhedron&,
			const body_position&
		);
		/// Finds the face that contains the given vertex that aligns with the given normal the most.
		[[nodiscard]] static u32 find_significant_face(
			const shapes::convex_polyhedron&, u32 contains_vert, vec3 normal_ls
		);
		/// Clips an edge loop against the given half space.
		[[nodiscard]] static contact_point_list clip_edge_loop_against_half_space(
			std::span<const vec3> verts, vec3 plane_normal, scalar plane_dist
		);
		/// Clips a face on the first polygon against all adjacent faces of a face on the second polygon.
		[[nodiscard]] static contact_point_list clip_face_against_polygon(
			const shapes::convex_polyhedron&,
			const body_position &pos1,
			u32 face1,
			const shapes::convex_polyhedron&,
			const body_position &pos2,
			u32 face2
		);

		/// Computes the contact manifold for the given contact.
		[[nodiscard]] static contact_manifold compute(const polyhedron_pair&, const epa::result&);

		short_vector<point, 6> points; ///< All contact points.
	};
}
