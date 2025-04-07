#include "lotus/collision/algorithms/gjk_epa.h"

/// \file
/// Implementation of the GJK and EPA algorithm.

#include <cassert>

#include "lotus/memory/stack_allocator.h"
#include "lotus/algorithms/convex_hull.h"

namespace lotus::collision {
	std::pair<bool, gjk_epa::gjk_result_state> gjk_epa::gjk() {
		auto bookmark = get_scratch_bookmark();

		gjk_result_state state = uninitialized;

		// compute simplex positions, which may have changed due to the bodies moving
		for (usize i = 0; i < simplex_vertices; ++i) {
			state.simplex_positions[i] = simplex_vertex_position(simplex[i]);
		}

		auto vertex_looked_at = bookmark.create_vector_array<bool>(
			polyhedron1->vertices.size() * polyhedron2->vertices.size(), false
		);
		auto mark_vert = [&](simplex_vertex v) {
			vertex_looked_at[v.index1 * polyhedron2->vertices.size() + v.index2] = true;
		};
		auto check_vert = [&](simplex_vertex v) {
			return vertex_looked_at[v.index1 * polyhedron2->vertices.size() + v.index2];
		};
		for (usize i = 0; i < simplex_vertices; ++i) {
			mark_vert(simplex[i]);
		}

		// find new vertices until we have a tetrahedron
		switch (simplex_vertices) {
		case 0:
			{
				// position is center1 - center2; support vector is its negation
				const vec3 initial_support_vec = position2.position - position1.position;
				simplex[0] = support_vertex(initial_support_vec);
				mark_vert(simplex[0]);
				state.simplex_positions[0] = simplex_vertex_position(simplex[0]);
				++simplex_vertices;
			}
			[[fallthrough]];
		case 1:
			{
				simplex[1] = support_vertex(-state.simplex_positions[0]);
				if (check_vert(simplex[1])) {
					return { false, state }; // this vertex is the closest to the origin - no collision
				}
				mark_vert(simplex[1]);
				state.simplex_positions[1] = simplex_vertex_position(simplex[1]);
				++simplex_vertices;

				// fast exit: the support vertex does not reach the origin, thus the Minkowski difference does not
				// contain the origin
				if (vec::dot(state.simplex_positions[0], state.simplex_positions[1]) > 0.0f) {
					return { false, state };
				}
			}
			[[fallthrough]];
		case 2:
			{
				const vec3 line_diff = state.simplex_positions[1] - state.simplex_positions[0];
				const vec3 support = vec::cross(line_diff, vec::cross(line_diff, state.simplex_positions[0]));
				simplex[2] = support_vertex(support);
				if (check_vert(simplex[2])) {
					return { false, state }; // no collision
				}
				mark_vert(simplex[2]);
				state.simplex_positions[2] = simplex_vertex_position(simplex[2]);
				++simplex_vertices;

				// fast exit
				if (vec::dot(support, state.simplex_positions[2]) < 0.0f) {
					return { false, state };
				}
			}
			[[fallthrough]];
		case 3:
			{
				vec3 support = vec::cross( // normal of the triangular face
					state.simplex_positions[1] - state.simplex_positions[0],
					state.simplex_positions[2] - state.simplex_positions[0]
				);
				if (vec::dot(support, state.simplex_positions[0]) > 0.0f) {
					support = -support;
				}
				simplex[3] = support_vertex(support);
				if (check_vert(simplex[3])) {
					return { false, state }; // no collision
				}
				mark_vert(simplex[3]);
				state.simplex_positions[3] = simplex_vertex_position(simplex[3]);
				++simplex_vertices;
				break;
			}
		}

		state.invert_even_normals = vec::dot(
			state.simplex_positions[3] - state.simplex_positions[0],
			vec::cross(
				state.simplex_positions[1] - state.simplex_positions[0],
				state.simplex_positions[2] - state.simplex_positions[0]
			)
		) > 0.0f;

		// the last face, whose normal is the direction that we last expanded in
		u32 prev_face = 4;

		while (true) {
			bool facing_origin = false;
			for (u32 i = 0; i < 4; ++i) {
				if (i == prev_face) {
					continue;
				}

				vec3 normal = vec::cross(
					state.simplex_positions[(i + 1) % 4] - state.simplex_positions[i],
					state.simplex_positions[(i + 2) % 4] - state.simplex_positions[i]
				);
				const bool invert_normal = (i % 2 == 0) == state.invert_even_normals;
				if (invert_normal) {
					normal = -normal;
				}

				// if the face is not facing towards the origin, we won't be expanding in its direction
				if (vec::dot(normal, state.simplex_positions[i]) >= 0.0f) {
					continue;
				}
				// otherwise, use its normal as the support vector to find the next vertex
				const simplex_vertex new_vertex = support_vertex(normal);
				if (check_vert(new_vertex)) {
					return { false, state }; // no more vertices to find; no collision
				}
				mark_vert(new_vertex);
				prev_face = i;

				// replace the vertex
				const usize replace_index = (i + 3) % 4;
				simplex[replace_index] = new_vertex;
				state.simplex_positions[replace_index] = simplex_vertex_position(new_vertex);

				// fast exit if the new vertex does not reach the origin
				if (vec::dot(normal, state.simplex_positions[replace_index]) <= 0.0f) {
					return { false, state };
				}

				facing_origin = true;
				break;
			}

			if (facing_origin) { // directly to the next iteration
				state.invert_even_normals = !state.invert_even_normals;
			} else {
				// none of the faces are facing the origin; it must be contained inside the tetrahedron
				return { true, state };
			}
		}
	}

	gjk_epa::epa_result gjk_epa::epa(gjk_result_state gjk_state) const {
		using convex_hull = incremental_convex_hull;

		auto bookmark = get_scratch_bookmark();

		auto hull_storage = convex_hull::create_storage_for_num_vertices(
			static_cast<u32>(polyhedron1->vertices.size() * polyhedron2->vertices.size()),
			bookmark.create_std_allocator<convex_hull::vec3>(),
			bookmark.create_std_allocator<convex_hull::face_entry>()
		);
		auto hull_data = hull_storage.create_user_data_storage<simplex_vertex, scalar>(
			uninitialized,
			uninitialized,
			bookmark.create_std_allocator<simplex_vertex>(),
			bookmark.create_std_allocator<scalar>()
		);
		auto hull = hull_storage.create_state_for_tetrahedron(
			{
				gjk_state.simplex_positions[0].into<convex_hull::scalar>(),
				gjk_state.simplex_positions[1].into<convex_hull::scalar>(),
				gjk_state.simplex_positions[2].into<convex_hull::scalar>(),
				gjk_state.simplex_positions[3].into<convex_hull::scalar>()
			},
			[&hull_data](const convex_hull::state &hull, convex_hull::face_id fi) {
				const auto& face = hull.get_face(fi);
				hull_data.get(fi) =
					vec::dot(vec::unsafe_normalize(face.normal), hull.get_vertex(face.vertex_indices[0]));
			}
		);
		for (u32 i = 0; i < 4; ++i) {
			hull_data.get(static_cast<convex_hull::vertex_id>(i)) = simplex[i];
		}

		convex_hull::face_id nearest_face_id = hull.get_any_face();
		scalar nearest_face_dist = hull_data.get(nearest_face_id);
		const usize num_verts = polyhedron1->vertices.size() * polyhedron2->vertices.size();
		for (usize iter = 4; iter < num_verts; ++iter) {
			// find the closest plane
			nearest_face_id = hull.get_any_face();
			nearest_face_dist = hull_data.get(nearest_face_id);
			for (
				convex_hull::face_id fi = hull.get_face(hull.get_any_face()).next;
				fi != hull.get_any_face();
				fi = hull.get_face(fi).next
			) {
				const scalar cur_dist = hull_data.get(fi);
				if (cur_dist < nearest_face_dist) {
					nearest_face_id = fi;
					nearest_face_dist = cur_dist;
				}
			}

			// numerical instability
			if (nearest_face_dist < 0.0f) {
				break;
			}

			// find new vertex
			const convex_hull::face &nearest_face = hull.get_face(nearest_face_id);
			const simplex_vertex new_vert_id = support_vertex(nearest_face.normal);
			const vec3 new_vert_pos = simplex_vertex_position(new_vert_id);
			const scalar new_dist = vec::dot(vec::unsafe_normalize(nearest_face.normal), new_vert_pos);
			if (new_dist - nearest_face_dist < 1e-6f) { // TODO: threshold
				break;
			}

			// expand & remove faces
			const convex_hull::vertex_id vi = hull.add_vertex_hint(new_vert_pos, nearest_face_id);
			hull_data.get(vi) = new_vert_id;
		}

		// result
		const convex_hull::face &result_face = hull.get_face(nearest_face_id);
		const convex_hull::vertex_id v1 = result_face.vertex_indices[0];
		const convex_hull::vertex_id v2 = result_face.vertex_indices[1];
		const convex_hull::vertex_id v3 = result_face.vertex_indices[2];
		return epa_result(
			std::array{ hull.get_vertex(v1), hull.get_vertex(v2), hull.get_vertex(v3) },
			std::array{ hull_data.get(v1), hull_data.get(v2), hull_data.get(v3) },
			result_face.normal,
			nearest_face_dist
		);
	}

	simplex_vertex gjk_epa::support_vertex(vec3 dir) const {
		return simplex_vertex(
			polyhedron1->get_support_vertex(position1.orientation.inverse().rotate(dir)).first,
			polyhedron2->get_support_vertex(-position2.orientation.inverse().rotate(dir)).first
		);
	}

	vec3 gjk_epa::simplex_vertex_position(simplex_vertex v) const {
		return
			position1.local_to_global(polyhedron1->vertices[v.index1].into<scalar>()) -
			position2.local_to_global(polyhedron2->vertices[v.index2].into<scalar>());
	}
}
