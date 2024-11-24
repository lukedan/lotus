#pragma once

/// \file
/// Polyhedrons.

#include <vector>

#include "lotus/math/vector.h"
#include "lotus/algorithms/convex_hull.h"
#include "lotus/collision/common.h"
#include "lotus/physics/body_properties.h"

namespace lotus::collision::shapes {
	/// A convex polyhedron defined by a series of vertices.
	struct polyhedron {
		/// Inertia matrix, center of mass, and volume of a polyhedron.
		struct properties {
			/// No initialization.
			properties(uninitialized_t) {
			}

			/// Computes the polyhedron properties for the given set of vertices and triangle faces.
			[[nodiscard]] static properties compute_for(
				std::span<const vec3>, std::span<const std::array<std::uint32_t, 3>>
			);

			/// The covariance matrix, computed with respect to the origin instead of the center of mass.
			mat33s covariance_matrix = uninitialized;
			vec3 center_of_mass = uninitialized; ///< Center of mass.
			scalar volume; ///< The volume of this object.

			/// Returns the \ref body_properties corresponding to this polyhedron with the given density.
			[[nodiscard]] physics::body_properties get_body_properties(scalar density) const {
				mat33s c = density * covariance_matrix;
				return physics::body_properties::create(mat33s::identity() * c.trace() - c, volume * density);
			}

			/// Returns the covariance matrix of this polyhedron translated by the given offset.
			[[nodiscard]] mat33s translated_covariance_matrix(vec3 dx) const {
				return covariance_matrix + volume * (
					dx * center_of_mass.transposed() +
					center_of_mass * dx.transposed() +
					mat::multiply_into_symmetric(dx, dx.transposed())
				);
			}
			/// Returns the properties of this polyhedron translated by the given offset.
			[[nodiscard]] properties translated(vec3 dx) const {
				properties result = *this;
				result.covariance_matrix = translated_covariance_matrix(dx);
				result.center_of_mass += dx;
				return result;
			}
		};

		std::vector<vec3> vertices; ///< Vertices of this polyhedron.

		/// Offsets this shape so that the center of mass is at the origin, and returns the resulting
		/// \ref body_properties.
		[[nodiscard]] physics::body_properties bake(scalar density) {
			auto bookmark = get_scratch_bookmark();

			// compute convex hull
			auto hull_storage = incremental_convex_hull::create_storage_for_num_vertices(
				static_cast<std::uint32_t>(vertices.size()),
				bookmark.create_std_allocator<incremental_convex_hull::vec3>(),
				bookmark.create_std_allocator<incremental_convex_hull::face_entry>()
			);
			auto hull_state = hull_storage.create_state_for_tetrahedron({
				vertices[0].into<float>(),
				vertices[1].into<float>(),
				vertices[2].into<float>(),
				vertices[3].into<float>()
			});
			for (std::size_t i = 4; i < vertices.size(); ++i) {
				hull_state.add_vertex(vertices[i].into<float>());
			}

			// gather faces
			auto faces = bookmark.create_reserved_vector_array<std::array<std::uint32_t, 3>>(
				incremental_convex_hull::get_max_num_triangles_for_vertex_count(
					static_cast<std::uint32_t>(vertices.size())
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
	};
}
