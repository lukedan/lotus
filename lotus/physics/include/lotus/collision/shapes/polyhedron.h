#pragma once

/// \file
/// Polyhedrons.

#include <vector>

#include "lotus/math/vector.h"
#include "lotus/algorithms/convex_hull.h"
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
				const std::vector<cvec3d>&, const std::vector<std::array<std::size_t, 3>>&
			);

			/// The covariance matrix, computed with respect to the origin instead of the center of mass.
			mat33d covariance_matrix = uninitialized;
			cvec3d center_of_mass = uninitialized; ///< Center of mass.
			double volume; ///< The volume of this object.

			/// Returns the \ref body_properties corresponding to this polyhedron with the given density.
			[[nodiscard]] physics::body_properties get_body_properties(double density) const {
				mat33d c = density * covariance_matrix;
				return physics::body_properties::create(mat33d::identity() * c.trace() - c, volume * density);
			}

			/// Returns the covariance matrix of this polyhedron translated by the given offset.
			[[nodiscard]] mat33d translated_covariance_matrix(cvec3d dx) const {
				return covariance_matrix + volume * (
					dx * center_of_mass.transposed() +
					center_of_mass * dx.transposed() +
					mat::multiply_into_symmetric(dx, dx.transposed())
				);
			}
			/// Returns the properties of this polyhedron translated by the given offset.
			[[nodiscard]] properties translated(cvec3d dx) const {
				properties result = *this;
				result.covariance_matrix = translated_covariance_matrix(dx);
				result.center_of_mass += dx;
				return result;
			}
		};

		std::vector<cvec3d> vertices; ///< Vertices of this polyhedron.

		/// Offsets this shape so that the center of mass is at the origin, and returns the resulting
		/// \ref body_properties.
		[[nodiscard]] physics::body_properties bake(double density) {
			/// No data associated with vertices or faces of the convex hull.
			struct _dummy {
				/// Uninitialized or whatever.
				_dummy(uninitialized_t) {
				}
			};
			using _hull_t = incremental_convex_hull<_dummy, _dummy>;

			auto dummy_data = [](const _hull_t&, const _hull_t::face&) {
			};

			auto hull = _hull_t::for_tetrahedron({
				_hull_t::vertex::create(vertices[0], uninitialized),
				_hull_t::vertex::create(vertices[1], uninitialized),
				_hull_t::vertex::create(vertices[2], uninitialized),
				_hull_t::vertex::create(vertices[3], uninitialized)
			}, dummy_data);
			for (std::size_t i = 4; i < vertices.size(); ++i) {
				hull.add_vertex(_hull_t::vertex::create(vertices[i], uninitialized), dummy_data);
			}
			std::vector<cvec3d> verts;
			for (auto &v : hull.vertices) {
				verts.emplace_back(v.position);
			}
			std::vector<std::array<std::size_t, 3>> faces;
			for (auto &f : hull.faces) {
				faces.emplace_back(f.vertex_indices);
			}
			properties prop = properties::compute_for(verts, faces);

			for (auto &v : vertices) {
				v -= prop.center_of_mass;
			}
			return prop.translated(-prop.center_of_mass).get_body_properties(density);
		}
	};
}
