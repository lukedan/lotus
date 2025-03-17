#include "lotus/collision/shapes/polyhedron.h"

/// \file
/// Implementation of polyhedron-related functions.

namespace lotus::collision::shapes {
	polyhedron::properties polyhedron::properties::compute_for(
		std::span<const vec3> verts, std::span<const std::array<u32, 3>> faces
	) {
		constexpr mat33s _canonical{
			{ 1.0f / 60.0f,  1.0f / 120.0f, 1.0f / 120.0f },
			{ 1.0f / 120.0f, 1.0f / 60.0f,  1.0f / 120.0f },
			{ 1.0f / 120.0f, 1.0f / 120.0f, 1.0f / 60.0f  }
		};

		polyhedron::properties result = uninitialized;
		result.center_of_mass    = zero;
		result.covariance_matrix = zero;
		result.volume            = zero;
		for (const auto &f : faces) {
			vec3 p1 = verts[f[0]];
			vec3 p2 = verts[f[1]];
			vec3 p3 = verts[f[2]];
			mat33s a = mat::concat_columns(p1, p2, p3);
			scalar det = mat::lup_decompose(a).determinant();
			result.covariance_matrix += det * mat::multiply_into_symmetric(a, _canonical * a.transposed());
			result.volume            += det;
			result.center_of_mass    += (det * 0.25f) * (p1 + p2 + p3);
		}
		result.center_of_mass /= result.volume;
		result.volume         /= 6.0f;
		return result;
	}


	physics::body_properties polyhedron::bake(scalar density) {
		auto bookmark = get_scratch_bookmark();

		// compute convex hull
		auto hull_storage = incremental_convex_hull::create_storage_for_num_vertices(
			static_cast<u32>(vertices.size()),
			bookmark.create_std_allocator<incremental_convex_hull::vec3>(),
			bookmark.create_std_allocator<incremental_convex_hull::face_entry>()
		);
		auto hull_state = hull_storage.create_state_for_tetrahedron({
			vertices[0].into<float>(),
			vertices[1].into<float>(),
			vertices[2].into<float>(),
			vertices[3].into<float>()
		});
		for (usize i = 4; i < vertices.size(); ++i) {
			hull_state.add_vertex(vertices[i].into<float>());
		}

		// gather faces
		auto faces = bookmark.create_reserved_vector_array<std::array<u32, 3>>(
			incremental_convex_hull::get_max_num_triangles_for_vertex_count(
				static_cast<u32>(vertices.size())
			)
		);
		{ // enumerate all faces
			auto face_ptr = hull_state.get_any_face();
			do {
				const incremental_convex_hull::face &face = hull_state.get_face(face_ptr);
				faces.emplace_back(std::array{
					std::to_underlying(face.vertex_indices[0]),
					std::to_underlying(face.vertex_indices[1]),
					std::to_underlying(face.vertex_indices[2])
				});
				face_ptr = face.next;
			} while (face_ptr != hull_state.get_any_face());
		}
		const auto prop = properties::compute_for(vertices, faces);

		for (vec3 &v : vertices) {
			v -= prop.center_of_mass;
		}
		return prop.translated(-prop.center_of_mass).get_body_properties(density);
	}

	std::pair<u32, scalar> polyhedron::get_support_vertex(vec3 dir) const {
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
}
