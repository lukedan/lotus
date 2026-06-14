#include "lotus/collision/shapes/convex_polyhedron.h"

/// \file
/// Implementation of polyhedron-related functions.

namespace lotus::collision::shapes {
	void convex_polyhedron::properties::add_face(vec3 p1, vec3 p2, vec3 p3) {
		constexpr static mat33s _canonical{
			{ 1.0f / 60.0f,  1.0f / 120.0f, 1.0f / 120.0f },
			{ 1.0f / 120.0f, 1.0f / 60.0f,  1.0f / 120.0f },
			{ 1.0f / 120.0f, 1.0f / 120.0f, 1.0f / 60.0f  }
		};

		const mat33s a = mat::concat_columns(p1, p2, p3);
		const scalar det = mat::lup_decompose(a).determinant();

		covariance_matrix += det * mat::multiply_into_symmetric(a, _canonical * a.transposed());
		sum_determinants  += det;
		weighted_vertices += det * (p1 + p2 + p3);
	}


	std::pair<convex_polyhedron, convex_polyhedron::properties> convex_polyhedron::bake(
		std::span<const vec3> vertices
	) {
		using convex_hull = incremental_convex_hull;

		auto bookmark = get_scratch_bookmark();

		// compute convex hull
		auto hull_storage = convex_hull::create_storage_for_num_vertices(
			static_cast<u32>(vertices.size()),
			bookmark.create_std_allocator<convex_hull::vec3>(),
			bookmark.create_std_allocator<convex_hull::face_entry>()
		);
		convex_hull::state hull_state = hull_storage.create_state_for_tetrahedron({
			vertices[0].into<convex_hull::scalar>(),
			vertices[1].into<convex_hull::scalar>(),
			vertices[2].into<convex_hull::scalar>(),
			vertices[3].into<convex_hull::scalar>()
		});
		for (usize i = 4; i < vertices.size(); ++i) {
			hull_state.add_vertex(vertices[i].into<convex_hull::scalar>());
		}

		// compute body properties, and mark all relevant vertices
		properties props = zero;
		std::vector vert_used = bookmark.create_vector_array<bool>(vertices.size(), false);
		hull_state.for_each_face([&](const convex_hull::face &f) {
			for (const convex_hull::vertex_id vid : f.vertex_indices) {
				vert_used[std::to_underlying(vid)] = true;
			}
			props.add_face(
				hull_state.get_vertex(f.vertex_indices[0]),
				hull_state.get_vertex(f.vertex_indices[1]),
				hull_state.get_vertex(f.vertex_indices[2])
			);
		});

		convex_polyhedron poly;

		// gather vertices
		std::vector vert_id_to_index =
			bookmark.create_vector_array<vertex_id>(hull_state.get_vertex_count(), vertex_id::invalid);
		const vec3 center_of_mass = props.get_center_of_mass();
		for (usize i = 0; i < hull_state.get_vertex_count(); ++i) {
			if (!vert_used[i]) {
				continue;
			}
			const vec3 vert = hull_state.get_vertex(static_cast<convex_hull::vertex_id>(i));
			vert_id_to_index[i] = static_cast<vertex_id>(poly.vertices.size());
			poly.vertices.emplace_back(vert - center_of_mass);
		}

		// gather faces
		hull_state.for_each_face([&](const convex_hull::face &f) {
			const vec3 n = vecu::normalize(f.normal);
			face *add_to_face = nullptr;
			for (face &poly_face : poly.faces) {
				if (vec::dot(poly_face.normal, n) > unique_normal_threshold) {
					// found a similar face - merge them
					add_to_face = &poly_face;
					break;
				}
			}
			if (!add_to_face) {
				// add new face
				add_to_face = &poly.faces.emplace_back(zero);
				add_to_face->normal = n;
			}
			// add verts to the new face
			for (const convex_hull::vertex_id vid : f.vertex_indices) {
				add_to_face->vertex_indices.emplace_back(vert_id_to_index[std::to_underlying(vid)]);
			}
		});
		// deduplicate and sort vertices in all faces
		for (face &f : poly.faces) {
			// deduplicate
			std::ranges::sort(f.vertex_indices);
			const auto [erase_beg, erase_end] = std::ranges::unique(f.vertex_indices);
			f.vertex_indices.erase(erase_beg, erase_end);

			// sort counter-clockwise
			const vec3 ref_point = poly.get_vertex(f.vertex_indices[0]);
			const auto sort_range = [&poly, &f, ref_point](this auto self, std::span<vertex_id> range) {
				if (range.size() < 2) {
					return;
				}
				const vec3 ref_axis = poly.get_vertex(range[0]) - ref_point;
				usize split = 0;
				for (usize i = 1; i < range.size(); ++i) {
					if (vec::dot(f.normal, vec::cross(ref_axis, poly.get_vertex(range[i]) - ref_point)) > 0.0f) {
						++split;
						std::swap(range[i], range[split]);
					}
				}
				std::swap(range[0], range[split]);
				self({ range.begin(), range.begin() + static_cast<std::span<u32>::difference_type>(split) });
				self({ range.begin() + static_cast<std::span<u32>::difference_type>(split + 1), range.end() });
			};
			// exclude the first element since it's used as a pivot
			sort_range({ f.vertex_indices.begin() + 1, f.vertex_indices.end() });
		}
		// find adjacent faces
		for (usize face_idx = 0; face_idx < poly.faces.size(); ++face_idx) {
			face &cur_face = poly.faces[face_idx];
			vertex_id prev_vert = cur_face.vertex_indices.back();
			for (const vertex_id vert : cur_face.vertex_indices) {
				// look for a face containing both prev_vert and vert
				for (usize other_face_idx = face_idx + 1; other_face_idx < poly.faces.size(); ++other_face_idx) {
					face &other_face = poly.faces[other_face_idx];
					bool has_prev_vert = false;
					bool has_vert = false;
					for (const vertex_id other_vert : other_face.vertex_indices) {
						if (other_vert == vert) {
							has_vert = true;
						}
						if (other_vert == prev_vert) {
							has_prev_vert = true;
						}
					}
					if (has_vert && has_prev_vert) {
						cur_face.adjacent_faces.emplace_back(static_cast<face_id>(other_face_idx));
						other_face.adjacent_faces.emplace_back(static_cast<face_id>(face_idx));
					}
				}
				prev_vert = vert;
			}
		}

		// gather unique normals by removing opposite facing normals
		for (const face &f : poly.faces) {
			bool has_similar = false;
			for (const vec3 v : poly.unique_face_normals) {
				if (vec::dot(v, f.normal) < -unique_normal_threshold) {
					has_similar = true;
					break;
				}
			}
			if (!has_similar) {
				poly.unique_face_normals.emplace_back(f.normal);
			}
		}

		// gather unique edges
		hull_state.for_each_face([&](const convex_hull::face &f) {
			const vec3 face_normal = vecu::normalize(f.normal);
			for (u32 i = 0; i < 3; ++i) {
				const convex_hull::half_edge_ref edge = f.edges[i];

				{ // if this face and its neighbor have similar normals, this edge does not need to be counted
					const convex_hull::face &other_face = hull_state.get_face(edge.face);
					const scalar dotv = vec::dot(face_normal, vecu::normalize(other_face.normal));
					if (dotv > unique_normal_threshold) {
						continue;
					}
				}

				const vec3 p1 = hull_state.get_vertex(f.vertex_indices[i]);
				const vec3 p2 = hull_state.get_vertex(f.vertex_indices[(i + 1) % 3]);
				const vec3 edge_dir = vecu::normalize(p2 - p1);
				// see if we have a similar edge
				bool has_similar = false;
				for (const vec3 e : poly.unique_edge_directions) {
					const scalar dotv = vec::dot(edge_dir, e);
					if (dotv > unique_edge_threshold || dotv < -unique_edge_threshold) {
						has_similar = true;
						break;
					}
				}

				if (!has_similar) {
					poly.unique_edge_directions.emplace_back(edge_dir);
				}
			}
		});

		return { std::move(poly), props };
	}

	std::pair<vertex_id, scalar> convex_polyhedron::get_support_vertex(vec3 dir) const {
		usize result = 0;
		scalar dot1max = vec::dot(vertices[result], dir);
		for (usize i = 1; i < vertices.size(); ++i) {
			const scalar dv = vec::dot(vertices[i], dir);
			if (dv > dot1max) {
				dot1max = dv;
				result = i;
			}
		}
		return { static_cast<vertex_id>(result), dot1max };
	}

	convex_polyhedron::axis_projection convex_polyhedron::project_onto_axis(vec3 dir) const {
		axis_projection result = std::nullopt;
		for (usize i = 0; i < vertices.size(); ++i) {
			const scalar dotv = vec::dot(dir, vertices[i]);
			if (dotv < result.min) {
				result.min = dotv;
				result.min_vertex = static_cast<vertex_id>(i);
			}
			if (dotv > result.max) {
				result.max = dotv;
				result.max_vertex = static_cast<vertex_id>(i);
			}
		}
		return result;
	}

	convex_polyhedron::axis_projection convex_polyhedron::project_onto_axis_with_transform(vec3 axis, body_position pos) const {
		const scalar offset = vec::dot(axis, pos.position);
		axis_projection result = project_onto_axis(pos.orientation.inverse().rotate(axis));
		result.min += offset;
		result.max += offset;
		return result;
	}
}
