#include "lotus/collision/algorithms/contact_manifold.h"

/// \file
/// Implementation of the contact manifold finding algorithm.

#include "lotus/logging.h"

namespace lotus::collision {
	std::pair<u32, u32> contact_manifold::compute_most_penetrating_vertices(
		const epa::result &epa_res,
		const shapes::convex_polyhedron &poly1,
		const body_position &pos1,
		const shapes::convex_polyhedron &poly2,
		const body_position &pos2
	) {
		const auto _find_most_penetrating_vertex = [](
			const shapes::convex_polyhedron &poly,
			uquats orientation,
			vec3 normal_ws,
			std::initializer_list<u32> candidates
		) -> u32 {
			const vec3 normal = orientation.conjugate().rotate(normal_ws);
			auto it = candidates.begin();
			u32 result = *it;
			scalar best_dot = vec::dot(normal, poly.vertices[*it]);
			for (++it; it != candidates.end(); ++it) {
				const scalar dot = vec::dot(normal, poly.vertices[*it]);
				if (dot > best_dot) {
					best_dot = dot;
					result = *it;
				}
			}
			return result;
		};

		switch (epa_res.compute_type()) {
		case epa::result::type::vertex_face:
			return {
				epa_res.vertices[0].index1,
				_find_most_penetrating_vertex(poly2, pos2.orientation, -epa_res.normal, {
					epa_res.vertices[0].index2, epa_res.vertices[1].index2, epa_res.vertices[2].index2
				})
			};
		case epa::result::type::face_vertex:
			return {
				_find_most_penetrating_vertex(poly1, pos1.orientation, epa_res.normal, {
					epa_res.vertices[0].index1, epa_res.vertices[1].index1, epa_res.vertices[2].index1
				}),
				epa_res.vertices[0].index2
			};
		case epa::result::type::edge_edge:
			return {
				_find_most_penetrating_vertex(poly1, pos1.orientation, epa_res.normal, {
					epa_res.vertices[0].index1,
					epa_res.vertices[epa_res.vertices[0].index1 == epa_res.vertices[1].index1 ? 2 : 1].index1
				}),
				_find_most_penetrating_vertex(poly2, pos2.orientation, -epa_res.normal, {
					epa_res.vertices[0].index2,
					epa_res.vertices[epa_res.vertices[0].index2 == epa_res.vertices[1].index2 ? 2 : 1].index2
				})
			};
		default:
			std::unreachable();
		}
	}

	u32 contact_manifold::find_significant_face(
		const shapes::convex_polyhedron &poly, u32 contains_vert, vec3 normal_ls
	) {
		u32 face_idx = std::numeric_limits<u32>::max();
		scalar best_dot = -2.0f;
		for (usize i = 0; i < poly.faces.size(); ++i) {
			const shapes::convex_polyhedron::face &cur_face = poly.faces[i];
			bool valid_face = false;
			for (const u32 vert : cur_face.vertex_indices) {
				if (vert == contains_vert) {
					valid_face = true;
					break;
				}
			}
			if (!valid_face) {
				continue;
			}
			const scalar cur_dot = vec::dot(poly.faces[i].normal, normal_ls);
			if (cur_dot > best_dot) {
				best_dot = cur_dot;
				face_idx = static_cast<u32>(i);
			}
		}
		return face_idx;
	}

	contact_manifold::contact_point_list contact_manifold::clip_edge_loop_against_half_space(
		std::span<const vec3> verts, vec3 plane_normal, scalar plane_dist
	) {
		contact_point_list result;
		vec3 prev_point = verts.back();
		bool prev_clipped = vec::dot(prev_point, plane_normal) > plane_dist;
		for (const vec3 cur_point : verts) {
			const bool cur_clipped = vec::dot(cur_point, plane_normal) > plane_dist;
			if (cur_clipped != prev_clipped) {
				//                     (t * prev + (1 - t) * cur) * normal = dist
				// t * (prev * normal) + cur * normal - t * (cur * normal) = dist
				//                      t * (prev * normal - cur * normal) = dist - cur * normal
				//
				// t = (dist - cur * normal) / ((prev - cur) * normal);
				const scalar t =
					(plane_dist - vec::dot(cur_point, plane_normal)) /
					vec::dot(prev_point - cur_point, plane_normal);
				if (std::isfinite(t)) {
					const vec3 intersection = t * prev_point + (1.0f - t) * cur_point;
					result.emplace_back(intersection);
				} else {
					// otherwise edge is almost parallel to the plane; add the point if necessary
					if (cur_clipped) {
						result.emplace_back(cur_point);
					}
				}
			}
			if (!cur_clipped) {
				result.emplace_back(cur_point);
			}
			prev_point = cur_point;
			prev_clipped = cur_clipped;
		}
		return result;
	}

	contact_manifold::contact_point_list contact_manifold::clip_face_against_polygon(
		const shapes::convex_polyhedron &poly1,
		const body_position &pos1,
		u32 face1,
		const shapes::convex_polyhedron &poly2,
		const body_position &pos2,
		u32 face2
	) {
		contact_point_list verts_ws;
		for (const u32 vert_idx : poly1.faces[face1].vertex_indices) {
			verts_ws.emplace_back(pos1.local_to_global(poly1.vertices[vert_idx]));
		}
		{ // clip against contact plane
			const shapes::convex_polyhedron::face &clip_face = poly2.faces[face2];
			const vec3 normal_ws = pos2.orientation.rotate(clip_face.normal);
			const scalar plane_dist =
				vec::dot(normal_ws, pos2.position) +
				vec::dot(clip_face.normal, poly2.vertices[clip_face.vertex_indices[0]]);
			verts_ws = clip_edge_loop_against_half_space(verts_ws, normal_ws, plane_dist);
		}
		for (const u32 face_idx : poly2.faces[face2].adjacent_faces) {
			const shapes::convex_polyhedron::face &clip_face = poly2.faces[face_idx];
			const vec3 normal_ws = pos2.orientation.rotate(clip_face.normal);
			const scalar plane_dist =
				vec::dot(normal_ws, pos2.position) +
				vec::dot(clip_face.normal, poly2.vertices[clip_face.vertex_indices[0]]);
			verts_ws = clip_edge_loop_against_half_space(verts_ws, normal_ws, plane_dist);
		}
		return verts_ws;
	}

	contact_manifold contact_manifold::compute_for_polyhedra(
		const polyhedron_pair &pair, const epa::result &epa_res
	) {
		const auto [vert_idx1, vert_idx2] =
			compute_most_penetrating_vertices(epa_res, *pair.shape1, pair.position1, *pair.shape2, pair.position2);
		const vec3 normal1_ls = pair.position1.orientation.conjugate().rotate(epa_res.normal);
		const u32 adj_face_idx1 = find_significant_face(*pair.shape1, vert_idx1, normal1_ls);
		const vec3 normal2_ls = pair.position2.orientation.conjugate().rotate(-epa_res.normal);
		const u32 adj_face_idx2 = find_significant_face(*pair.shape2, vert_idx2, normal2_ls);

		// clip faces
		const contact_point_list clipped_verts1_ws = clip_face_against_polygon(
			*pair.shape1, pair.position1, adj_face_idx1, *pair.shape2, pair.position2, adj_face_idx2
		);
		const contact_point_list clipped_verts2_ws = clip_face_against_polygon(
			*pair.shape2, pair.position2, adj_face_idx2, *pair.shape1, pair.position1, adj_face_idx1
		);

		// project each vertex for each polyhedron onto the other polyhedron
		short_vector<std::pair<vec3, vec3>, 6> candidates_ws;
		const shapes::convex_polyhedron::face &adj_face1 = pair.shape1->faces[adj_face_idx1];
		const shapes::convex_polyhedron::face &adj_face2 = pair.shape2->faces[adj_face_idx2];
		{
			const vec3 vert2_ws = pair.position2.local_to_global(pair.shape2->vertices[vert_idx2]);
			const vec3 face_normal2_ws = pair.position2.orientation.rotate(adj_face2.normal);
			for (const vec3 v1 : clipped_verts1_ws) {
				const scalar depth = vec::dot(v1 - vert2_ws, face_normal2_ws);
				if (depth < 0.0f) { // discard non-penetrating contacts
					candidates_ws.emplace_back(v1, v1 - depth * face_normal2_ws);
				}
			}
		}
		{
			const vec3 vert1_ws = pair.position1.local_to_global(pair.shape1->vertices[vert_idx1]);
			const vec3 face_normal1_ws = pair.position1.orientation.rotate(adj_face1.normal);
			for (const vec3 v2 : clipped_verts2_ws) {
				const scalar depth = vec::dot(v2 - vert1_ws, face_normal1_ws);
				if (depth < 0.0f) { // discard non-penetrating contacts
					candidates_ws.emplace_back(v2 - depth * face_normal1_ws, v2);
				}
			}
		}

		// deduplicate vertices
		// TODO

		// gather results
		contact_manifold result;
		for (const std::pair<vec3, vec3> candidate : candidates_ws) {
			result.points.emplace_back(
				pair.position1.global_to_local(candidate.first),
				pair.position2.global_to_local(candidate.second)
			);
		}
		result.normal = epa_res.normal;
		return result;
	}
}
