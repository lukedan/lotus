#include "lotus/collision/algorithms/contact_manifold.h"

/// \file
/// Implementation of the contact manifold finding algorithm.

#include "lotus/logging.h"

namespace lotus::collision {
	std::pair<vertex_id, vertex_id> contact_manifold::compute_most_penetrating_vertices(
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
			std::initializer_list<vertex_id> candidates
		) -> vertex_id {
			const vec3 normal = orientation.conjugate().rotate(normal_ws);
			auto it = candidates.begin();
			vertex_id result = *it;
			scalar best_dot = vec::dot(normal, poly.get_vertex(*it));
			for (++it; it != candidates.end(); ++it) {
				const scalar dot = vec::dot(normal, poly.get_vertex(*it));
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

	face_id contact_manifold::find_significant_face(
		const shapes::convex_polyhedron &poly, vertex_id contains_vert, vec3 normal_ls
	) {
		face_id face_idx = face_id::invalid;
		scalar best_dot = -2.0f;
		for (usize i = 0; i < poly.faces.size(); ++i) {
			const shapes::convex_polyhedron::face &cur_face = poly.faces[i];
			bool valid_face = false;
			for (const vertex_id vert : cur_face.vertex_indices) {
				if (vert == contains_vert) {
					valid_face = true;
					break;
				}
			}
			if (!valid_face) {
				continue;
			}
			const scalar cur_dot = vec::dot(cur_face.normal, normal_ls);
			if (cur_dot > best_dot) {
				best_dot = cur_dot;
				face_idx = static_cast<face_id>(i);
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
				// if t is out of range or infinite, the edge is almost parallel to the plane
				// we ignore it since there's bound to be another clipping point
				if (t >= 0.0f && t <= 1.0f) {
					const vec3 intersection = t * prev_point + (1.0f - t) * cur_point;
					result.emplace_back(intersection);
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
		face_id face1,
		const shapes::convex_polyhedron &poly2,
		const body_position &pos2,
		face_id face2
	) {
		contact_point_list verts_ws;
		for (const vertex_id vert_idx : poly1.get_face(face1).vertex_indices) {
			verts_ws.emplace_back(pos1.local_to_global(poly1.get_vertex(vert_idx)));
		}
		const shapes::convex_polyhedron::face &clip_face = poly2.get_face(face2);
		const vec3 clip_face_normal_ws = pos2.orientation.rotate(clip_face.normal);
		vec3 last_vert_ws = pos2.local_to_global(poly2.get_vertex(clip_face.vertex_indices.back()));
		for (const vertex_id vert_idx : clip_face.vertex_indices) {
			const vec3 cur_vert_ws = pos2.local_to_global(poly2.get_vertex(vert_idx));
			const vec3 normal_ws = vec::cross(clip_face_normal_ws, cur_vert_ws - last_vert_ws);
			const scalar plane_dist = vec::dot(cur_vert_ws, normal_ws);
			verts_ws = clip_edge_loop_against_half_space(verts_ws, normal_ws, plane_dist);
			last_vert_ws = cur_vert_ws;
		}
		return verts_ws;
	}

	contact_manifold contact_manifold::compute_for_polyhedra(
		const polyhedron_pair &pair, const epa::result &epa_res
	) {
		// TODO prioritize the face that's better aligned?
		const auto [vert_idx1, vert_idx2] =
			compute_most_penetrating_vertices(epa_res, *pair.shape1, pair.position1, *pair.shape2, pair.position2);
		const vec3 normal1_ls = pair.position1.orientation.conjugate().rotate(epa_res.normal);
		const face_id adj_face_idx1 = find_significant_face(*pair.shape1, vert_idx1, normal1_ls);
		const vec3 normal2_ls = pair.position2.orientation.conjugate().rotate(-epa_res.normal);
		const face_id adj_face_idx2 = find_significant_face(*pair.shape2, vert_idx2, normal2_ls);

		// clip faces
		const contact_point_list clipped_verts1_ws = clip_face_against_polygon(
			*pair.shape1, pair.position1, adj_face_idx1, *pair.shape2, pair.position2, adj_face_idx2
		);

		// project each vertex for each polyhedron onto the other polyhedron
		short_vector<std::pair<vec3, vec3>, 6> candidates_ws;
		const shapes::convex_polyhedron::face &adj_face2 = pair.shape2->get_face(adj_face_idx2);
		{
			const vec3 vert2_ws = pair.position2.local_to_global(pair.shape2->get_vertex(vert_idx2));
			const vec3 face_normal2_ws = pair.position2.orientation.rotate(adj_face2.normal);
			for (const vec3 v1 : clipped_verts1_ws) {
				const scalar depth = vec::dot(v1 - vert2_ws, face_normal2_ws);
				if (depth < 0.0f) { // discard non-penetrating contacts
					candidates_ws.emplace_back(v1, v1 - depth * face_normal2_ws);
				}
			}
		}

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
