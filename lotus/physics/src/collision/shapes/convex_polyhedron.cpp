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


	std::pair<convex_polyhedron, convex_polyhedron::properties> convex_polyhedron::bake(std::span<const vec3> vertices) {
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

		properties props = zero;
		std::vector<vec3> unique_face_normals; // TODO allocator
		std::vector<vec3> unique_edges; // TODO allocator
		{
			convex_hull::face_id face_ptr = hull_state.get_any_face();
			do {
				const convex_hull::face &face = hull_state.get_face(face_ptr);
				const vec3 face_normal = vecu::normalize(face.normal);

				// compute body properties
				props.add_face(
					hull_state.get_vertex(face.vertex_indices[0]),
					hull_state.get_vertex(face.vertex_indices[1]),
					hull_state.get_vertex(face.vertex_indices[2])
				);

				{ // gather all unique faces
					// check if we have a similar normal
					bool has_similar = false;
					for (const vec3 n : unique_face_normals) {
						const scalar dotv = vec::dot(face_normal, n);
						if (dotv > unique_normal_threshold || dotv < -unique_normal_threshold) {
							has_similar = true;
							break;
						}
					}
					if (!has_similar) {
						unique_face_normals.emplace_back(face_normal);
					}
				}

				// gather all unique edges
				for (u32 i = 0; i < 3; ++i) {
					const convex_hull::half_edge_ref edge = face.edges[i];

					{ // if this face and its neighbor have similar normals, this edge does not need to be counted
						const convex_hull::face &other_face = hull_state.get_face(edge.face);
						const scalar dotv = vec::dot(face_normal, vecu::normalize(other_face.normal));
						if (dotv > unique_normal_threshold) {
							continue;
						}
					}

					const vec3 p1 = hull_state.get_vertex(face.vertex_indices[i]);
					const vec3 p2 = hull_state.get_vertex(face.vertex_indices[(i + 1) % 3]);
					const vec3 edge_dir = vecu::normalize(p2 - p1);
					// see if we have a similar edge
					bool has_similar = false;
					for (const vec3 e : unique_edges) {
						const scalar dotv = vec::dot(edge_dir, e);
						if (dotv > unique_edge_threshold || dotv < -unique_edge_threshold) {
							has_similar = true;
							break;
						}
					}

					if (!has_similar) {
						unique_edges.emplace_back(edge_dir);
					}
				}

				face_ptr = face.next;
			} while (face_ptr != hull_state.get_any_face());
		}

		convex_polyhedron poly;

		// gather vertices
		const vec3 center_of_mass = props.get_center_of_mass();
		for (usize i = 0; i < hull_state.get_vertex_count(); ++i) {
			const vec3 vert = hull_state.get_vertex(static_cast<convex_hull::vertex_id>(i));
			poly.vertices.emplace_back(vert - center_of_mass);
		}

		poly.unique_face_normals.insert(
			poly.unique_face_normals.end(), unique_face_normals.begin(), unique_face_normals.end()
		);
		poly.unique_edge_directions.insert(
			poly.unique_edge_directions.end(), unique_edges.begin(), unique_edges.end()
		);

		// TODO gather faces

		return { std::move(poly), props };
	}

	std::pair<u32, scalar> convex_polyhedron::get_support_vertex(vec3 dir) const {
		usize result = 0;
		scalar dot1max = vec::dot(vertices[result], dir);
		for (usize i = 1; i < vertices.size(); ++i) {
			const scalar dv = vec::dot(vertices[i], dir);
			if (dv > dot1max) {
				dot1max = dv;
				result = i;
			}
		}
		return { static_cast<u32>(result), dot1max };
	}

	convex_polyhedron::axis_projection convex_polyhedron::project_onto_axis(vec3 dir) const {
		axis_projection result = std::nullopt;
		for (usize i = 0; i < vertices.size(); ++i) {
			const scalar dotv = vec::dot(dir, vertices[i]);
			if (dotv < result.min) {
				result.min = dotv;
				result.min_vertex = static_cast<u32>(i);
			}
			if (dotv > result.max) {
				result.max = dotv;
				result.max_vertex = static_cast<u32>(i);
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
