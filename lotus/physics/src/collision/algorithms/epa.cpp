#include "lotus/collision/algorithms/epa.h"

/// \file
/// Implementation of the expanding polytope algorithm.

#include "lotus/algorithms/convex_hull.h"
#include "lotus/collision/shapes/convex_polyhedron.h"

namespace lotus::collision {
	epa_t::result epa_t::operator()(polyhedron_pair input, gjk_t::result gjk_state) {
		using convex_hull = incremental_convex_hull;

		auto bookmark = get_scratch_bookmark();

		auto hull_storage = convex_hull::create_storage_for_num_vertices(
			static_cast<u32>(input.shape1->vertices.size() * input.shape2->vertices.size()),
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
				gjk_state.transient.simplex_positions[0].into<convex_hull::scalar>(),
				gjk_state.transient.simplex_positions[1].into<convex_hull::scalar>(),
				gjk_state.transient.simplex_positions[2].into<convex_hull::scalar>(),
				gjk_state.transient.simplex_positions[3].into<convex_hull::scalar>()
			},
			[&hull_data](const convex_hull::state &hull, convex_hull::face_id fi) {
				const auto& face = hull.get_face(fi);
				hull_data.get(fi) =
					vec::dot(vec::unsafe_normalize(face.normal), hull.get_vertex(face.vertex_indices[0]));
			}
		);
		for (u32 i = 0; i < 4; ++i) {
			hull_data.get(static_cast<convex_hull::vertex_id>(i)) = gjk_state.persistent.simplex[i];
		}

		convex_hull::face_id nearest_face_id = hull.get_any_face();
		scalar nearest_face_dist = hull_data.get(nearest_face_id);
		const usize num_verts = input.shape1->vertices.size() * input.shape2->vertices.size();
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
			const simplex_vertex new_vert_id = input.support_vertex(nearest_face.normal);
			const vec3 new_vert_pos = input.simplex_vertex_position(new_vert_id);
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
		return result(
			std::array{ hull.get_vertex(v1), hull.get_vertex(v2), hull.get_vertex(v3) },
			std::array{ hull_data.get(v1), hull_data.get(v2), hull_data.get(v3) },
			result_face.normal,
			nearest_face_dist
		);
	}
}
