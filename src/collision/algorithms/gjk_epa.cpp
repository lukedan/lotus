#include "lotus/collision/algorithms/gjk_epa.h"

/// \file
/// Implementation of the GJK and EPA algorithm.

#include <cassert>

#include "lotus/utils/stack_allocator.h"
#include "lotus/algorithms/convex_hull.h"

namespace lotus::collision {
	std::pair<bool, gjk_epa::gjk_result_state> gjk_epa::gjk() {
		gjk_result_state state = uninitialized;

		// compute simplex positions, which may have changed due to the bodies moving
		for (std::size_t i = 0; i < simplex_vertices; ++i) {
			state.simplex_positions[i] = simplex_vertex_position(simplex[i]);
		}

		auto vertex_looked_at = stack_allocator::for_this_thread().create_vector_array<std::uint8_t>(
			polyhedron1->vertices.size() * polyhedron2->vertices.size(), static_cast<uint8_t>(0)
		);
		auto mark_vert = [&](simplex_vertex v) {
			vertex_looked_at[v.index1 * polyhedron2->vertices.size() + v.index2] = 1;
		};
		auto check_vert = [&](simplex_vertex v) {
			return vertex_looked_at[v.index1 * polyhedron2->vertices.size() + v.index2] != 0;
		};
		for (std::size_t i = 0; i < simplex_vertices; ++i) {
			mark_vert(simplex[i]);
		}

		// find new vertices until we have a tetrahedron
		switch (simplex_vertices) {
		case 0:
			{
				// position is center1 - center2; support vector is its negation
				cvec3d initial_support_vec = center2 - center1;
				simplex[0] = support_vertex(initial_support_vec);
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
				if (vec::dot(state.simplex_positions[0], state.simplex_positions[1]) > 0.0) {
					return { false, state };
				}
			}
			[[fallthrough]];
		case 2:
			{
				cvec3d line_diff = state.simplex_positions[1] - state.simplex_positions[0];
				cvec3d support = vec::cross(line_diff, vec::cross(line_diff, state.simplex_positions[0]));
				simplex[2] = support_vertex(support);
				if (check_vert(simplex[2])) {
					return { false, state }; // no collision
				}
				mark_vert(simplex[2]);
				++simplex_vertices;
				state.simplex_positions[2] = simplex_vertex_position(simplex[2]);

				// fast exit
				if (vec::dot(support, state.simplex_positions[2]) < 0.0) {
					return { false, state };
				}
			}
			[[fallthrough]];
		case 3:
			{
				cvec3d support = vec::cross( // normal of the triangular face
					state.simplex_positions[1] - state.simplex_positions[0],
					state.simplex_positions[2] - state.simplex_positions[0]
				);
				if (vec::dot(support, state.simplex_positions[0]) > 0.0) {
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
		) > 0.0;

		while (true) {
			bool facing_origin = false;
			for (std::size_t i = 0; i < 4; ++i) {
				cvec3d normal = vec::cross(
					state.simplex_positions[(i + 1) % 4] - state.simplex_positions[i],
					state.simplex_positions[(i + 2) % 4] - state.simplex_positions[i]
				);
				bool invert_normal = (i % 2 == 0) == state.invert_even_normals;
				if (invert_normal) {
					normal = -normal;
				}
				double dotv = vec::dot(normal, state.simplex_positions[i]);
				if (dotv < 0.0) {
					// this face is facing the origin; use its normal as the support vector to find the next vertex
					auto new_vertex = support_vertex(normal);
					if (check_vert(new_vertex)) {
						return { false, state }; // no more vertices to find; no collision
					}
					mark_vert(new_vertex);
					// replace the vertex
					std::size_t replace_index = (i + 3) % 4;
					simplex[replace_index] = new_vertex;
					state.simplex_positions[replace_index] = simplex_vertex_position(new_vertex);

					// fast exit
					if (vec::dot(normal, state.simplex_positions[replace_index]) <= 0.0) {
						return { false, state };
					}

					facing_origin = true;
					break;
				}
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
		using _convex_hull_t = incremental_convex_hull<simplex_vertex, double, stack_allocator::allocator>;

		auto compute_face_data = [](const _convex_hull_t &hull, _convex_hull_t::face &f) {
			f.data = vec::dot(f.normal, hull.vertices[f.vertex_indices[0]].position);
		};

		std::array<_convex_hull_t::vertex, 4> initial_vertices{
			uninitialized, uninitialized, uninitialized, uninitialized
		};
		for (std::size_t i = 0; i < 4; ++i) {
			initial_vertices[i] = _convex_hull_t::vertex::create(gjk_state.simplex_positions[i], simplex[i]);
		}
		_convex_hull_t convex_hull = _convex_hull_t::for_tetrahedron(
			std::move(initial_vertices), compute_face_data,
			stack_allocator::allocator<_convex_hull_t::vertex>::for_this_thread(),
			stack_allocator::allocator<_convex_hull_t::face>::for_this_thread()
		);

		while (true) {
			// find the closest plane
			double nearest_face_dist = convex_hull.faces.front().data;
			auto nearest_face = convex_hull.faces.begin();
			for (auto it = nearest_face; it != convex_hull.faces.end(); ++it) {
				if (it->data < nearest_face_dist) {
					nearest_face_dist = it->data;
					nearest_face = it;
				}
			}
			assert(nearest_face_dist >= 0.0);

			// find new vertex
			simplex_vertex new_vert_id = support_vertex(nearest_face->normal);
			auto new_vert = _convex_hull_t::vertex::create(simplex_vertex_position(new_vert_id), new_vert_id);
			double new_dist = vec::dot(nearest_face->normal, new_vert.position);
			if (new_dist - nearest_face->data < 1e-6) { // TODO threshold
				auto &vert1 = convex_hull.vertices[nearest_face->vertex_indices[0]];
				auto &vert2 = convex_hull.vertices[nearest_face->vertex_indices[1]];
				auto &vert3 = convex_hull.vertices[nearest_face->vertex_indices[2]];
				return epa_result::create(
					{ vert1.position, vert2.position, vert3.position },
					{ vert1.data, vert2.data, vert3.data },
					nearest_face->normal, nearest_face->data
				);
			}

			// expand & remove faces
			convex_hull.add_vertex_hint(std::move(new_vert), nearest_face, compute_face_data);
		}
	}

	gjk_epa::simplex_vertex gjk_epa::support_vertex(cvec3d dir) const {
		gjk_epa::simplex_vertex result = uninitialized;

		// polyhedron 1
		cvec3d dir1 = orient1.inverse().rotate(dir);
		result.index1 = 0;
		double dot1max = vec::dot(polyhedron1->vertices[0], dir1);
		for (std::size_t i = 1; i < polyhedron1->vertices.size(); ++i) {
			double dv = vec::dot(polyhedron1->vertices[i], dir1);
			if (dv > dot1max) {
				dot1max = dv;
				result.index1 = i;
			}
		}

		// polyhedron 2
		cvec3d dir2 = orient2.inverse().rotate(dir);
		result.index2 = 0;
		double dot2min = vec::dot(polyhedron2->vertices[0], dir2);
		for (std::size_t i = 1; i < polyhedron2->vertices.size(); ++i) {
			double dv = vec::dot(polyhedron2->vertices[i], dir2);
			if (dv < dot2min) {
				dot2min = dv;
				result.index2 = i;
			}
		}

		return result;
	}

	cvec3d gjk_epa::simplex_vertex_position(simplex_vertex v) const {
		return
			(center1 + orient1.rotate(polyhedron1->vertices[v.index1])) -
			(center2 + orient2.rotate(polyhedron2->vertices[v.index2]));
	}
}
