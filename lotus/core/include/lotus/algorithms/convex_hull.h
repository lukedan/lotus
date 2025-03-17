#pragma once

/// \file
/// Implementation of 3D convex hull algorithms.

#include <optional>
#include <span>
#include <vector>

#include "lotus/common.h"
#include "lotus/containers/pool.h"
#include "lotus/math/vector.h"
#include "lotus/memory/stack_allocator.h"
#include "lotus/utils/static_function.h"

namespace lotus {
	/// Implementation of an incremental convex hull algorithm.
	struct incremental_convex_hull {
	public:
		/// Prevent objects of this type from being created.
		incremental_convex_hull() = delete;
		/// Prevent objects of this type from being created.
		~incremental_convex_hull() = delete;

		/// Scalar type.
		using scalar = float;
		/// Vector type.
		using vec3 = cvec3<scalar>;

		/// Opaque index type for vertex IDs.
		enum class vertex_id : u32 {
		};
		/// Opaque index type for face IDs.
		enum class face_id : u32 {
			invalid = 0x3FFFFFFFu ///< Invalid value - not the maximum value to avoid generating warnings later.
		};

		/// Opaque index type for face vertex indices.
		enum class face_vertex_ref : u32 {
			invalid = 3, ///< Invalid value.
		};
		/// Returns the next vertex in a face. The reference must be valid.
		[[nodiscard]] static constexpr face_vertex_ref next_in_face(face_vertex_ref v) {
			crash_if(v >= face_vertex_ref::invalid);
			return static_cast<face_vertex_ref>((std::to_underlying(v) + 1) % 3);
		}
		/// Returns the previous vertex in a face. The reference must be valid.
		[[nodiscard]] static constexpr face_vertex_ref previous_in_face(face_vertex_ref v) {
			crash_if(v >= face_vertex_ref::invalid);
			return static_cast<face_vertex_ref>((std::to_underlying(v) + 2) % 3);
		}

		/// A reference to a half edge.
		struct half_edge_ref {
			/// No initialization.
			half_edge_ref(uninitialized_t) {
			}
			/// Initializes this reference to empty.
			constexpr half_edge_ref(std::nullptr_t) :
				face(static_cast<face_id>(0)), vertex(face_vertex_ref::invalid) {
			}
			/// Initializes all fields of this struct.
			constexpr half_edge_ref(face_id f, face_vertex_ref v) : face(f), vertex(v) {
			}

			/// Returns the refence to the next half edge in the triangle.
			[[nodiscard]] constexpr half_edge_ref next_in_face() const {
				return half_edge_ref(face, incremental_convex_hull::next_in_face(vertex));
			}
			/// Returns the refence to the previous half edge in the triangle.
			[[nodiscard]] constexpr half_edge_ref previous_in_face() const {
				return half_edge_ref(face, incremental_convex_hull::previous_in_face(vertex));
			}

			/// Returns whether this reference is valid.
			[[nodiscard]] constexpr bool is_valid() const {
				return vertex != face_vertex_ref::invalid;
			}
			[[nodiscard]] constexpr explicit operator bool() const {
				return is_valid();
			}

			/// Equality.
			[[nodiscard]] friend bool operator==(half_edge_ref, half_edge_ref) = default;

			face_id         face   : 30; ///< The face that owns this half edge.
			face_vertex_ref vertex : 2;  ///< The vertex that this half edge starts from.
		};

		/// A triangular face.
		struct face {
			/// No initialization.
			face(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			constexpr face(std::array<vertex_id, 3> vert_ids, vec3 n) :
				normal(n),
				vertex_indices(vert_ids),
				edges{ nullptr, nullptr, nullptr },
				previous(face_id::invalid),
				next(face_id::invalid) {
			}

			/// Returns the ID of the given vertex.
			[[nodiscard]] constexpr vertex_id get_vertex(face_vertex_ref r) const {
				return vertex_indices[std::to_underlying(r)];
			}

			vec3 normal = uninitialized; ///< The unnormalized normal vector of this face.
			std::array<vertex_id, 3> vertex_indices; ///< Vertex indices in counter-clockwise order.
			/// Half-edges of this face. Each half edge starts from the corresponding vertex of \ref vertex_indices
			/// and ends at the next vertex in the list, wrapping around if necessary.
			std::array<half_edge_ref, 3> edges{ uninitialized, uninitialized, uninitialized };
			face_id previous; ///< Previous face. These form a circular doubly-linked list.
			face_id next; ///< Next face. These form a circular doubly-linked list.
		};
		using face_entry = pool_entry<face, face_id>; ///< Pool entry type for a face.

		/// Tag type indicating that no data is associated with an element.
		struct no_data {
		};
		/// Grants access to user data.
		template <
			typename VertexData, typename FaceData, typename VertexAllocator, typename FaceAllocator
		> struct user_data {
		public:
			/// Indicates whether there's vertex data.
			constexpr static bool has_vertex_data = !std::is_same_v<VertexData, no_data>;
			/// Indicates whether there's face data.
			constexpr static bool has_face_data = !std::is_same_v<FaceData, no_data>;

			/// Initializes all fields of this struct.
			user_data(
				u32 vert_count,
				u32 face_count,
				const VertexData &vert_data,
				const FaceData &face_data,
				const VertexAllocator &vert_alloc,
				const FaceAllocator &face_alloc
			) : _verts(vert_count, vert_data, vert_alloc), _faces(face_count, face_data, face_alloc) {
			}
			/// Initializes only vertex data.
			template <
				typename T = int, std::enable_if_t<has_vertex_data && !has_face_data, T> = 0
			> explicit user_data(u32 vert_count, std::span<VertexData> v) : _verts(vert_count, v) {
			}
			/// Initializes only face data.
			template <
				typename T = int, std::enable_if_t<has_face_data && !has_vertex_data, T> = 0
			> explicit user_data(u32 face_count, std::span<FaceData> f) : _faces(face_count, f) {
			}

			/// Returns the vertex data associated with the given ID.
			template <
				typename T = int, std::enable_if_t<has_vertex_data, T> = 0
			> [[nodiscard]] VertexData &get(vertex_id i) {
				return (*_verts)[std::to_underlying(i)];
			}
			/// \overload
			template <
				typename T = int, std::enable_if_t<has_vertex_data, T> = 0
			> [[nodiscard]] const VertexData &get(vertex_id i) const {
				return (*_verts)[std::to_underlying(i)];
			}
			/// Returns the face data associated with the given ID.
			template <
				typename T = int, std::enable_if_t<has_face_data, T> = 0
			> [[nodiscard]] FaceData &get(face_id i) {
				return (*_faces)[std::to_underlying(i)];
			}
			/// \overload
			template <
				typename T = int, std::enable_if_t<has_face_data, T> = 0
			> [[nodiscard]] const FaceData &get(face_id i) const {
				return (*_faces)[std::to_underlying(i)];
			}
		private:
			static_optional<std::vector<VertexData, VertexAllocator>, has_vertex_data> _verts; ///< Vertices.
			static_optional<std::vector<FaceData, FaceAllocator>, has_face_data> _faces; ///< Faces.
		};
		/// Creates a new \ref user_data object.
		template <
			typename VertexData, typename FaceData, typename VertexAllocator, typename FaceAllocator
		> [[nodiscard]] inline static user_data<
			VertexData, FaceData, VertexAllocator, FaceAllocator
		> create_user_data_storage(
			u32 vert_count,
			u32 face_count,
			const VertexData &vert_data,
			const FaceData &face_data,
			const VertexAllocator &vert_alloc,
			const FaceAllocator &face_alloc
		) {
			return user_data<VertexData, FaceData, VertexAllocator, FaceAllocator>(
				vert_count, face_count, vert_data, face_data, vert_alloc, face_alloc
			);
		}

		/// Computes incremental convex hull for a set of vertices.
		struct state {
		public:
			/// Callback for face addition/removal events.
			using face_callback = static_function<void(const state&, face_id)>;

			/// Initializes this object to empty.
			state(std::nullptr_t) {
			}
			/// Creates a new convex hull for the given tetrahedron.
			[[nodiscard]] static state for_tetrahedron(
				std::array<vec3, 4> initial_verts,
				std::span<vec3> vert_storage,
				std::span<face_entry> face_storage,
				face_callback face_added = nullptr,
				face_callback face_removing = nullptr
			);
			/// Move constructor.
			state(state&&);
			/// Move assignment.
			state &operator=(state&&);
			/// Calls \ref _free_all_faces().
			~state();

			/// Adds a vertex to this convex hull. The iterator provides a face that faces the new vertex.
			vertex_id add_vertex_hint(vec3, face_id hint);
			/// Adds a new vertex to the polytope. This may return \p std::nullopt if the vertex is already inside the
			/// convex hull, in which case the vertex will not be recorded at all.
			std::optional<vertex_id> add_vertex(vec3);

			/// Returns the number of vertices that have been recorded.
			[[nodiscard]] usize get_vertex_count() const {
				return _num_verts_added;
			}
			/// Returns a vertex in the polyhedra.
			[[nodiscard]] vec3 get_vertex(vertex_id i) const {
				crash_if(std::to_underlying(i) >= _num_verts_added);
				return _vertices[std::to_underlying(i)];
			}
			/// Returns a face in the polyhedra.
			[[nodiscard]] const face &get_face(face_id i) const {
				return _faces_pool[i];
			}
			/// Returns a reference to an arbitrary face in the polyhedra that can be used to enumerate the faces.
			[[nodiscard]] face_id get_any_face() const {
				return _any_face;
			}
			/// Returns the maximum number of vertices.
			[[nodiscard]] u32 get_vertex_capacity() const {
				return static_cast<u32>(_vertices.size());
			}
			/// Returns the maximum number of faces.
			[[nodiscard]] u32 get_face_capacity() const {
				return static_cast<u32>(_faces.size());
			}

			/// Callback that's invoked after a new face has been added.
			face_callback on_face_added    = nullptr;
			/// Callback that's invoked before a face is being removed.
			face_callback on_face_removing = nullptr;
		private:
			std::span<vec3> _vertices; ///< Vertices.
			usize _num_verts_added = 0; ///< Number of vertices that have been added.
			std::span<face_entry> _faces; ///< Storage for faces.
			pool_manager<face_entry> _faces_pool = nullptr; ///< Pool of faces.
			face_id _any_face = face_id::invalid; ///< Pointer to any face in the geometry.

			/// Initializes the vertex and face storage references.
			state(
				std::span<vec3> vs,
				std::span<face_entry> fs,
				face_callback face_added,
				face_callback face_removing
			) :
				on_face_added(std::move(face_added)),
				on_face_removing(std::move(face_removing)),
				_vertices(vs),
				_faces(fs),
				_faces_pool(_faces) {
			}

			/// Adds a vertex to the list.
			[[nodiscard]] vertex_id _add_vertex(vec3);
			/// Creates a new face and computes its normal and user data.
			[[nodiscard]] face_id _add_face(std::array<vertex_id, 3>);
			/// Removes a face from the convex hull.
			void _remove_face(face_id);

			/// Frees all faces in the pool.
			void _free_all_faces();
		};

		/// Returns the maximum possible number of triangular faces in a polyhedra with \p n vertices.
		/// https://math.stackexchange.com/questions/4579790/how-many-faces-can-a-polyhedron-with-n-vertices-have
		[[nodiscard]] inline static constexpr u32 get_max_num_triangles_for_vertex_count(u32 n) {
			return 2 * n - 4;
		}

		/// Provides storage for the convex hull algorithm.
		template <
			typename VertAllocator = std::allocator<vec3>, typename FaceAllocator = std::allocator<face_entry>
		> struct storage {
		public:
			/// Creates enough storage for creating a polyhedra with the given number of vertices.
			[[nodiscard]] inline static storage create_for_num_vertices(
				u32 n, const VertAllocator &vert_alloc, const FaceAllocator &face_alloc
			) {
				return storage(n, vert_alloc, face_alloc);
			}

			/// Creates user data storage for the same upper bound of the number of vertices and faces.
			template <
				typename VertexData, typename FaceData, typename VertexDataAllocator, typename FaceDataAllocator
			> [[nodiscard]] user_data<VertexData, FaceData, VertexDataAllocator, FaceDataAllocator> create_user_data_storage(
				const VertexData &vert_data,
				const FaceData &face_data,
				const VertexDataAllocator &vert_alloc,
				const FaceDataAllocator &face_alloc
			) {
				return incremental_convex_hull::create_user_data_storage<VertexData, FaceData>(
					static_cast<u32>(_vertices.size()),
					static_cast<u32>(_faces.size()),
					vert_data,
					face_data,
					vert_alloc,
					face_alloc
				);
			}
			/// Creates an algorithm state object for the given initial tetrahedron. Two state objects must not
			/// simultaneously exist using the same underlying storage.
			[[nodiscard]] state create_state_for_tetrahedron(
				std::array<vec3, 4> verts,
				state::face_callback face_added = nullptr,
				state::face_callback face_removing = nullptr
			) {
				return state::for_tetrahedron(verts, _vertices, _faces, std::move(face_added), std::move(face_removing));
			}
		private:
			std::vector<vec3, VertAllocator> _vertices; ///< Vertices.
			std::vector<face_entry, FaceAllocator> _faces; ///< Faces.

			/// Initializes all vectors.
			storage(u32 n, const VertAllocator &vert_alloc, const FaceAllocator &face_alloc) :
				_vertices(n, zero, vert_alloc),
				_faces(face_entry::make_storage(get_max_num_triangles_for_vertex_count(n), face_alloc)) {
			}
		};
		/// Shorthand for \ref storage::create_for_num_vertices().
		template <
			typename VertAllocator = std::allocator<vec3>, typename FaceAllocator = std::allocator<face_entry>
		> [[nodiscard]] inline static storage<VertAllocator, FaceAllocator> create_storage_for_num_vertices(
			u32 n,
			const VertAllocator &vert_alloc = VertAllocator(),
			const FaceAllocator &face_alloc = FaceAllocator()
		) {
			return storage<VertAllocator, FaceAllocator>::create_for_num_vertices(n, vert_alloc, face_alloc);
		}
	};
}
