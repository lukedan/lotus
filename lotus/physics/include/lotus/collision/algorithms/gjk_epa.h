#include "lotus/math/matrix.h"

/// \file
/// The GJK and EPA algorithms.

#include <array>

#include "lotus/memory/stack_allocator.h"
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
			simplex_vertex(std::uint32_t i1, std::uint32_t i2) : index1(i1), index2(i2) {
			}

			/// Equality.
			friend bool operator==(simplex_vertex, simplex_vertex) = default;

			std::uint32_t index1; ///< Vertex index in the first polyhedron.
			std::uint32_t index2; ///< Vertex index in the second polyhedron.
		};
		/// State of the GJK algorithm used by the EPA algorithm. This should not be kept between timesteps and is
		/// invalidated when the bodies move.
		struct gjk_result_state {
			/// No initialization.
			gjk_result_state(uninitialized_t) {
			}

			/// Vertex positions of the simplex.
			std::array<vec3, 4> simplex_positions{
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
			/// Initializes all fields of this struct.
			epa_result(std::array<vec3, 3> positions, std::array<simplex_vertex, 3> verts, vec3 n, scalar d) :
				simplex_positions(positions), vertices(verts), normal(n), penetration_depth(d) {
			}

			/// Positions of \ref vertices.
			std::array<vec3, 3> simplex_positions{ uninitialized, uninitialized, uninitialized };
			/// Vertices of the contact plane.
			std::array<simplex_vertex, 3> vertices{ uninitialized, uninitialized, uninitialized };
			vec3 normal = uninitialized; ///< Contact normal.
			scalar penetration_depth; ///< Penetration depth.
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
		[[nodiscard]] simplex_vertex support_vertex(vec3) const;
		/// Returns the position, in global coordinates, of the given \ref simplex_vertex.
		[[nodiscard]] vec3 simplex_vertex_position(simplex_vertex) const;

		/// Computes the transformed vertex positions of the given polyhedron.
		template <
			template <typename> typename Allocator = std::allocator
		> [[nodiscard]] inline static std::vector<vec3, Allocator<vec3>> compute_vertices(
			uquats orient, vec3 center, const shapes::polyhedron &poly
		) {
			std::vector<vec3, Allocator<vec3>> result(poly.vertices.size(), uninitialized);
			for (std::size_t i = 0; i < poly.vertices.size(); ++i) {
				result[i] = center + orient.rotate(poly.vertices[i]);
			}
			return result;
		}

		/// Vertices of the simplex.
		std::array<simplex_vertex, 4> simplex{ uninitialized, uninitialized, uninitialized, uninitialized };
		std::size_t simplex_vertices; ///< The number of valid vertices in \ref simplex.

		uquats orient1 = uninitialized; ///< Orientation of \ref polyhedron1.
		uquats orient2 = uninitialized; ///< Orientation of \ref polyhedron2.
		vec3 center1 = uninitialized; ///< Offset of \ref polyhedron1.
		vec3 center2 = uninitialized; ///< Offset of \ref polyhedron2.
		const shapes::polyhedron *polyhedron1; ///< The first polyhedron.
		const shapes::polyhedron *polyhedron2; ///< The second polyhedron.
	};
}
