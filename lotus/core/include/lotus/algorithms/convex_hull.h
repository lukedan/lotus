#pragma once

/// \file
/// Implementation of 3D convex hull algorithms.

#include <vector>
#include <list>
#include <stack>
#include <deque>
#include <array>

#include "lotus/utils/stack_allocator.h"
#include "lotus/common.h"

namespace lotus {
	/// Computes incremental convex hull for a set of vertices.
	template <
		typename VertexData, typename FaceData, template <typename> typename Allocator = std::allocator
	> struct incremental_convex_hull {
	public:
		struct face;
		using face_collection = std::list<face, Allocator<face>>; ///< The collection of faces.
		/// A vertex.
		struct vertex {
		public:
			/// No initialization.
			vertex(uninitialized_t) {
			}
			/// Creates a new vertex.
			[[nodiscard]] constexpr static vertex create(cvec3d pos, VertexData data) {
				return vertex(pos, std::move(data));
			}

			cvec3d position = uninitialized; ///< The position of this vertex.
			[[no_unique_address]] VertexData data = uninitialized; ///< User data for this vertex.
		protected:
			/// Initializes all fields.
			constexpr vertex(cvec3d pos, VertexData d) : position(pos), data(std::move(d)) {
			}
		};
		/// Reference to a half-edge.
		struct half_edge_ref {
		public:
			/// The value of \ref index that indicates that this reference is empty.
			constexpr static auto null_index = std::numeric_limits<std::uint8_t>::max();

			/// No initialization.
			half_edge_ref(uninitialized_t) {
			}
			/// Initializes \ref index to \ref null_index.
			half_edge_ref(std::nullptr_t) : index(null_index) {
			}
			/// Creates a new \ref half_edge_ref to the given edge int he given face.
			[[nodiscard]] inline static half_edge_ref to(typename face_collection::iterator f, std::uint8_t i) {
				return half_edge_ref(f, i);
			}

			/// Returns the next edge in \ref face.
			[[nodiscard]] half_edge_ref next() const {
				return to(face, static_cast<std::uint8_t>((index + 1) % 3));
			}
			/// Returns the previous edge in \ref face.
			[[nodiscard]] half_edge_ref prev() const {
				return to(face, static_cast<std::uint8_t>((index + 2) % 3));
			}

			/// Dereferencing.
			[[nodiscard]] half_edge_ref &operator*() const {
				return face->edges[index];
			}
			/// Dereferencing.
			[[nodiscard]] half_edge_ref *operator->() const {
				return &face->edges[index];
			}

			/// Tests whether this iterator is empty.
			[[nodiscard]] bool empty() const {
				return index == null_index;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return !empty();
			}

			/// Equality.
			[[nodiscard]] friend bool operator==(half_edge_ref, half_edge_ref) = default;

			typename face_collection::iterator face; ///< The face that contains the half edge.
			std::uint8_t index; ///< The index of this edge in \ref face::edges.
		protected:
			/// Initializes all fields of this reference.
			half_edge_ref(typename face_collection::iterator f, std::uint8_t i) : face(f), index(i) {
			}
		};
		/// A triangular face.
		struct face {
		public:
			/// No initialization.
			face(uninitialized_t) {
			}
			/// Creates a new object with empty neighbor references.
			[[nodiscard]] constexpr static face create(
				std::array<std::size_t, 3> vert_ids, cvec3d n, FaceData data
			) {
				return face(vert_ids, n, std::move(data));
			}
			/// Creates a new object with empty neighbor references and uninitialized user data.
			[[nodiscard]] inline static face create_without_data(std::array<std::size_t, 3> vert_ids, cvec3d n) {
				return face(vert_ids, n);
			}

			std::array<std::size_t, 3> vertex_indices; ///< Vertex indices in counter-clockwise order.
			/// Half-edges of this face.
			std::array<half_edge_ref, 3> edges{
				uninitialized, uninitialized, uninitialized
			};
			cvec3d normal = uninitialized; ///< The normalized normal of this face.
			[[no_unique_address]] FaceData data = uninitialized; ///< User data for this face.
			bool marked; ///< Marker used by the convex hull algorithm.
		protected:
			/// Initializes all fields of this struct.
			constexpr face(std::array<std::size_t, 3> vert_ids, cvec3d n, FaceData fd) :
				vertex_indices(vert_ids), edges{ nullptr, nullptr, nullptr },
				normal(n), data(std::move(fd)), marked(false) {
			}
			/// Initializes all fields apart from \ref data.
			face(std::array<std::size_t, 3> vert_ids, cvec3d n) : face(vert_ids, n, uninitialized) {
			}
		};

		/// Creates an empty object.
		[[nodiscard]] inline static incremental_convex_hull create_empty(
			const Allocator<vertex> &vert_alloc = Allocator<vertex>(),
			const Allocator<face> &face_alloc = Allocator<face>()
		) {
			return incremental_convex_hull(vert_alloc, face_alloc);
		}
		/// Creates a new convex hull for the given tetrahedron.
		template <typename ComputeFaceData> [[nodiscard]] inline static incremental_convex_hull for_tetrahedron(
			std::array<vertex, 4> verts, const ComputeFaceData &face_data,
			const Allocator<vertex> &vert_alloc = Allocator<vertex>(),
			const Allocator<face> &face_alloc = Allocator<face>()
		) {
			incremental_convex_hull result(vert_alloc, face_alloc);
			result.vertices = std::vector<vertex, Allocator<vertex>>(
				std::make_move_iterator(verts.begin()), std::make_move_iterator(verts.end()), vert_alloc
			);

			bool invert_even_normals = vec::dot(
				vec::cross(
					result.vertices[1].position - result.vertices[0].position,
					result.vertices[2].position - result.vertices[0].position
				),
				result.vertices[3].position - result.vertices[0].position
			) > 0.0;

			// create faces
			std::array<typename face_collection::iterator, 4> faces;
			{
				std::array<std::array<std::size_t, 3>, 4> vertex_indices;
				if (invert_even_normals) {
					vertex_indices = { { { 0, 2, 1 }, { 1, 2, 3 }, { 2, 0, 3 }, { 3, 0, 1 } } };
				} else {
					vertex_indices = { { { 0, 1, 2 }, { 1, 3, 2 }, { 2, 3, 0 }, { 3, 1, 0 } } };
				}
				for (std::size_t i = 0; i < 4; ++i) {
					faces[i] = result.faces.insert(
						result.faces.end(), result.create_face(vertex_indices[i], face_data)
					);
				}
			}

			{ // initialize references
				std::array<std::array<std::pair<std::size_t, std::size_t>, 3>, 4> neighbor_indices;
				if (invert_even_normals) {
					// { { 0, 2, 1 }, { 1, 2, 3 }, { 2, 0, 3 }, { 3, 0, 1 } }
					neighbor_indices = { {
						{ { { 2, 0 }, { 1, 0 }, { 3, 1 } } },
						{ { { 0, 1 }, { 2, 2 }, { 3, 2 } } },
						{ { { 0, 0 }, { 3, 0 }, { 1, 1 } } },
						{ { { 2, 1 }, { 0, 2 }, { 1, 2 } } }
					} };
				} else {
					// { { 0, 1, 2 }, { 1, 3, 2 }, { 2, 3, 0 }, { 3, 1, 0 } }
					neighbor_indices = { {
						{ { { 3, 1 }, { 1, 2 }, { 2, 2 } } },
						{ { { 3, 0 }, { 2, 0 }, { 0, 1 } } },
						{ { { 1, 1 }, { 3, 2 }, { 0, 2 } } },
						{ { { 1, 0 }, { 0, 0 }, { 2, 1 } } }
					} };
				}
				for (std::size_t i = 0; i < 4; ++i) {
					for (std::size_t j = 0; j < 3; ++j) {
						auto [face_id, vert_id] = neighbor_indices[i][j];
						faces[i]->edges[j] = half_edge_ref::to(faces[face_id], static_cast<std::uint8_t>(vert_id));
					}
				}
			}

			return result;
		}

		/// Adds a vertex to this convex hull. The iterator provides a face that faces the new vertex.
		template <typename ComputeFaceData> void add_vertex_hint(
			vertex v, typename face_collection::iterator hint, const ComputeFaceData &compute_data
		) {
			using _ptr = typename face_collection::iterator;
			using _ptr_deque = std::deque<_ptr, stack_allocator::std_allocator<_ptr>>;

			std::size_t vert_id = vertices.size();
			auto &vert = vertices.emplace_back(std::move(v));

			half_edge_ref boundary_edge = uninitialized;
			{ // find all faces that should be removed & create new faces
				auto bookmark = stack_allocator::for_this_thread().bookmark();
				std::stack<_ptr, _ptr_deque> stk{ _ptr_deque(bookmark.create_std_allocator<_ptr>()) };
				stk.emplace(hint);
				hint->marked = true;
				while (!stk.empty()) {
					_ptr cur = stk.top();
					stk.pop();
					for (std::size_t i = 0; i < 3; ++i) {
						auto other_half_edge = cur->edges[i];
						if (!other_half_edge) {
							continue;
						}
						if (other_half_edge.face->marked) {
							*other_half_edge = nullptr;
							continue;
						}
						bool delete_face = vec::dot(
							other_half_edge.face->normal,
							vert.position - vertices[other_half_edge.face->vertex_indices[0]].position
						) > 0.0;
						if (delete_face) {
							stk.emplace(other_half_edge.face);
							other_half_edge.face->marked = true;
						} else {
							// we've found a boundary edge
							boundary_edge = other_half_edge;
						}
						// make sure there are no more references to cur, since we're gonna delete it
						*other_half_edge = nullptr;
					}
					// remove this face from `faces`
					faces.erase(cur);
				}
			}

			// update references of all newly-created faces
			auto ref = boundary_edge;
			half_edge_ref last_new_face = nullptr;
			half_edge_ref first_new_face = nullptr;
			do {
				// create new face
				auto new_face = faces.insert(faces.end(), create_face(
					{ ref.face->vertex_indices[(ref.index + 1) % 3], ref.face->vertex_indices[ref.index], vert_id },
					compute_data
				));

				// update references
				new_face->edges[0] = ref;
				*ref = half_edge_ref::to(new_face, 0);
				if (last_new_face) {
					new_face->edges[1] = last_new_face;
					*last_new_face = half_edge_ref::to(new_face, 1);
				} else {
					first_new_face = half_edge_ref::to(new_face, 1);
				}
				last_new_face = half_edge_ref::to(new_face, 2);

				// update loop variable
				ref = ref.next();
				while (*ref && ref != boundary_edge) {
					ref = ref->next();
				}
			} while (ref != boundary_edge);
			*first_new_face = last_new_face;
			*last_new_face = first_new_face;
		}
		/// Adds a new vertex to the polytope.
		template <typename ComputeFaceData> void add_vertex(vertex v, const ComputeFaceData &compute_data) {
			for (auto it = faces.begin(); it != faces.end(); ++it) {
				if (vec::dot(it->normal, v.position - vertices[it->vertex_indices[0]].position) > 0.0) {
					add_vertex_hint(std::move(v), it, compute_data);
					return;
				}
			}
		}

		/// Creates a new face and computes its normal and user data.
		template <typename ComputeFaceData> [[nodiscard]] face create_face(
			std::array<std::size_t, 3> verts, const ComputeFaceData &compute_data
		) const {
			cvec3d normal = vec::unsafe_normalize(vec::cross(
				vertices[verts[1]].position - vertices[verts[0]].position,
				vertices[verts[2]].position - vertices[verts[0]].position
			));
			auto result = face::create_without_data(verts, normal);
			compute_data(*this, result);
			return result;
		}

		std::vector<vertex, Allocator<vertex>> vertices; ///< Vertices.
		face_collection faces; ///< Faces.
	protected:
		/// Initializes the data structures with the corresponding allocators.
		explicit incremental_convex_hull(const Allocator<vertex> &vert_alloc, const Allocator<face> &face_alloc) :
			vertices(vert_alloc), faces(face_alloc) {
		}
	};
}
