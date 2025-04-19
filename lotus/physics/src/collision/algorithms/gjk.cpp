#include "lotus/collision/algorithms/gjk.h"

/// \file
/// Implementation of the GJK and EPA algorithm.

#include <cassert>

#include "lotus/memory/stack_allocator.h"
#include "lotus/collision/shapes/polyhedron.h"

namespace lotus::collision {
	gjk_t::result gjk_t::operator()(polyhedron_pair input, persistent_result pstate) {
		auto bookmark = get_scratch_bookmark();

		transient_result tstate = uninitialized;

		// compute simplex positions, which may have changed due to the bodies moving
		for (usize i = 0; i < pstate.simplex_vertices; ++i) {
			tstate.simplex_positions[i] = input.simplex_vertex_position(pstate.simplex[i]);
		}

		auto vertex_looked_at = bookmark.create_vector_array<bool>(
			input.shape1->vertices.size() * input.shape2->vertices.size(), false
		);
		auto mark_vert = [&](simplex_vertex v) {
			vertex_looked_at[v.index1 * input.shape2->vertices.size() + v.index2] = true;
		};
		auto check_vert = [&](simplex_vertex v) {
			return vertex_looked_at[v.index1 * input.shape2->vertices.size() + v.index2];
		};
		for (usize i = 0; i < pstate.simplex_vertices; ++i) {
			mark_vert(pstate.simplex[i]);
		}

		// find new vertices until we have a tetrahedron
		switch (pstate.simplex_vertices) {
		case 0:
			{
				// position is center1 - center2; support vector is its negation
				const vec3 initial_support_vec = input.position2.position - input.position1.position;
				pstate.simplex[0] = input.support_vertex(initial_support_vec);
				mark_vert(pstate.simplex[0]);
				tstate.simplex_positions[0] = input.simplex_vertex_position(pstate.simplex[0]);
				++pstate.simplex_vertices;
			}
			[[fallthrough]];
		case 1:
			{
				pstate.simplex[1] = input.support_vertex(-tstate.simplex_positions[0]);
				if (check_vert(pstate.simplex[1])) { // this vertex is the closest to the origin - no collision
					return result::does_not_intersect(pstate, tstate);
				}
				mark_vert(pstate.simplex[1]);
				tstate.simplex_positions[1] = input.simplex_vertex_position(pstate.simplex[1]);
				++pstate.simplex_vertices;

				// fast exit: the support vertex does not reach the origin, thus the Minkowski difference does not
				// contain the origin
				if (vec::dot(tstate.simplex_positions[0], tstate.simplex_positions[1]) > 0.0f) {
					return result::does_not_intersect(pstate, tstate);
				}
			}
			[[fallthrough]];
		case 2:
			{
				const vec3 line_diff = tstate.simplex_positions[1] - tstate.simplex_positions[0];
				const vec3 support = vec::cross(line_diff, vec::cross(line_diff, tstate.simplex_positions[0]));
				pstate.simplex[2] = input.support_vertex(support);
				if (check_vert(pstate.simplex[2])) {
					return result::does_not_intersect(pstate, tstate); // no collision
				}
				mark_vert(pstate.simplex[2]);
				tstate.simplex_positions[2] = input.simplex_vertex_position(pstate.simplex[2]);
				++pstate.simplex_vertices;

				// fast exit
				if (vec::dot(support, tstate.simplex_positions[2]) < 0.0f) {
					return result::does_not_intersect(pstate, tstate);
				}
			}
			[[fallthrough]];
		case 3:
			{
				vec3 support = vec::cross( // normal of the triangular face
					tstate.simplex_positions[1] - tstate.simplex_positions[0],
					tstate.simplex_positions[2] - tstate.simplex_positions[0]
				);
				if (vec::dot(support, tstate.simplex_positions[0]) > 0.0f) {
					support = -support;
				}
				pstate.simplex[3] = input.support_vertex(support);
				if (check_vert(pstate.simplex[3])) {
					return result::does_not_intersect(pstate, tstate); // no collision
				}
				mark_vert(pstate.simplex[3]);
				tstate.simplex_positions[3] = input.simplex_vertex_position(pstate.simplex[3]);
				++pstate.simplex_vertices;
				break;
			}
		}

		tstate.invert_even_normals = vec::dot(
			tstate.simplex_positions[3] - tstate.simplex_positions[0],
			vec::cross(
				tstate.simplex_positions[1] - tstate.simplex_positions[0],
				tstate.simplex_positions[2] - tstate.simplex_positions[0]
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
					tstate.simplex_positions[(i + 1) % 4] - tstate.simplex_positions[i],
					tstate.simplex_positions[(i + 2) % 4] - tstate.simplex_positions[i]
				);
				const bool invert_normal = (i % 2 == 0) == tstate.invert_even_normals;
				if (invert_normal) {
					normal = -normal;
				}

				// if the face is not facing towards the origin, we won't be expanding in its direction
				if (vec::dot(normal, tstate.simplex_positions[i]) >= 0.0f) {
					continue;
				}
				// otherwise, use its normal as the support vector to find the next vertex
				const simplex_vertex new_vertex = input.support_vertex(normal);
				if (check_vert(new_vertex)) {
					return result::does_not_intersect(pstate, tstate); // no more vertices to find; no collision
				}
				mark_vert(new_vertex);
				prev_face = i;

				// replace the vertex
				const usize replace_index = (i + 3) % 4;
				pstate.simplex[replace_index] = new_vertex;
				tstate.simplex_positions[replace_index] = input.simplex_vertex_position(new_vertex);

				// fast exit if the new vertex does not reach the origin
				if (vec::dot(normal, tstate.simplex_positions[replace_index]) <= 0.0f) {
					return result::does_not_intersect(pstate, tstate);
				}

				facing_origin = true;
				break;
			}

			if (facing_origin) { // directly to the next iteration
				tstate.invert_even_normals = !tstate.invert_even_normals;
			} else {
				// none of the faces are facing the origin; it must be contained inside the tetrahedron
				return result::intersects(pstate, tstate);
			}
		}
	}
}
