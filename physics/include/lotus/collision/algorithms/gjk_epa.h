#include "lotus/math/matrix.h"

/// \file
/// The GJK and EPA algorithms.

#include <array>

#include "lotus/utils/stack_allocator.h"
#include "lotus/math/quaternion.h"
#include "lotus/collision/shapes/polyhedron.h"
#include "lotus/physics/body.h"

namespace lotus::collision {
	/// Implementation of the Gilbert-Johnson-Keerthi algorithm and the expanding polytope algorithm.
	struct gjk_epa {
		/// A vertex in a simplex.
		struct simplex_vertex {
			/// No initialization.
			simplex_vertex(uninitialized_t) {
			}
			/// Creates a vertex from the given indices.
			[[nodiscard]] inline static simplex_vertex from_indices(std::size_t i1, std::size_t i2) {
				simplex_vertex result = uninitialized;
				result.index1 = i1;
				result.index2 = i2;
				return result;
			}

			/// Equality.
			friend bool operator==(simplex_vertex, simplex_vertex) = default;

			std::size_t index1; ///< Vertex index in the first polyhedron.
			std::size_t index2; ///< Vertex index in the second polyhedron.
		};
		/// State of the GJK algorithm used by the EPA algorithm. This should not be kept between timesteps and is
		/// invalidated when the bodies move.
		struct gjk_result_state {
			/// No initialization.
			gjk_result_state(uninitialized_t) {
			}

			/// Vertex positions of the simplex.
			std::array<cvec3d, 4> simplex_positions{
				uninitialized, uninitialized, uninitialized, uninitialized
			};
			/// Indicates whether the normals of faces at even indices need to be inverted. This is only valid when a
			/// simplex has been successfully created by the GJK algorithm, i.e., when \ref gjk() returns \p true
			/// (there may be other cases where this is valid but this is usually not relevant in those cases).
			bool invert_even_normals;
		};
		/// Results from the expanding polytope algorithm.
		struct epa_result {
			/// No initialization.
			epa_result(uninitialized_t) {
			}
			/// Creates a \ref epa_result object.
			[[nodiscard]] inline static epa_result create(
				std::array<cvec3d, 3> positions, std::array<simplex_vertex, 3> verts, cvec3d n, double d
			) {
				epa_result result = uninitialized;
				result.simplex_positions = positions;
				result.vertices = verts;
				result.normal = n;
				result.depth = d;
				return result;
			}

			std::array<cvec3d, 3> simplex_positions{
				uninitialized, uninitialized, uninitialized
			}; ///< Positions of \ref vertices.
			std::array<simplex_vertex, 3> vertices{
				uninitialized, uninitialized, uninitialized
			}; ///< Vertices of the contact plane.
			cvec3d normal = uninitialized; ///< Contact normal.
			double depth; ///< Penetration depth.
		};

		/// No initialization.
		gjk_epa(uninitialized_t) {
		}
		/// Creates a new object for the given pair of bodies.
		[[nodiscard]] inline static gjk_epa for_bodies(
			const physics::body_state &st1, const shapes::polyhedron &s1,
			const physics::body_state &st2, const shapes::polyhedron &s2
		) {
			gjk_epa result = uninitialized;
			result.simplex_vertices = 0;
			result.orient1 = st1.rotation;
			result.orient2 = st2.rotation;
			result.center1 = st1.position;
			result.center2 = st2.position;
			result.polyhedron1 = &s1;
			result.polyhedron2 = &s2;
			return result;
		}

		/// Updates and returns the result of the GJK algorithm.
		[[nodiscard]] std::pair<bool, gjk_result_state> gjk();
		/// The expanding polytope algorithm.
		[[nodiscard]] epa_result epa(gjk_result_state) const;

		/// Returns the support vertex for the given direction.
		[[nodiscard]] simplex_vertex support_vertex(cvec3d) const;
		/// Returns the position, in global coordinates, of the given \ref simplex_vertex.
		[[nodiscard]] cvec3d simplex_vertex_position(simplex_vertex) const;

		/// Computes the transformed vertex positions of the given polyhedron.
		template <
			template <typename> typename Allocator = std::allocator
		> [[nodiscard]] inline static std::vector<cvec3d, Allocator<cvec3d>> compute_vertices(
			uquatd orient, cvec3d center, const shapes::polyhedron &poly
		) {
			std::vector<cvec3d, Allocator<cvec3d>> result(poly.vertices.size(), uninitialized);
			for (std::size_t i = 0; i < poly.vertices.size(); ++i) {
				result[i] = center + orient.rotate(poly.vertices[i]);
			}
			return result;
		}

		/// Vertices of the simplex.
		std::array<simplex_vertex, 4> simplex{
			uninitialized, uninitialized, uninitialized, uninitialized
		};
		std::size_t simplex_vertices; ///< The number of valid vertices in \ref simplex.

		uquatd orient1 = uninitialized; ///< Orientation of \ref polyhedron1.
		uquatd orient2 = uninitialized; ///< Orientation of \ref polyhedron2.
		cvec3d center1 = uninitialized; ///< Offset of \ref polyhedron1.
		cvec3d center2 = uninitialized; ///< Offset of \ref polyhedron2.
		const shapes::polyhedron *polyhedron1; ///< The first polyhedron.
		const shapes::polyhedron *polyhedron2; ///< The second polyhedron.
	};
}
