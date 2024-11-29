#include "lotus/algorithms/convex_hull.h"

#include <stack>

namespace lotus::incremental_convex_hull {
	state state::for_tetrahedron(
		std::array<vec3, 4> verts,
		std::span<vec3> vert_storage,
		std::span<face_entry> face_storage,
		face_callback face_added,
		face_callback face_removing
	) {
		state result(vert_storage, face_storage, std::move(face_added), std::move(face_removing));

		const bool invert_even_normals =
			vec::dot(vec::cross(verts[1] - verts[0], verts[2] - verts[0]), verts[3] - verts[0]) > 0.0f;

		std::array<vertex_id, 4> vert_ids;
		for (std::size_t i = 0; i < 4; ++i) {
			vert_ids[i] = result._add_vertex(verts[i]);
		}

		// create faces
		std::array<face_id, 4> faces;
		{
			std::array<std::array<std::uint32_t, 3>, 4> vertex_indices;
			if (invert_even_normals) {
				vertex_indices = { { { 0, 2, 1 }, { 1, 2, 3 }, { 2, 0, 3 }, { 3, 0, 1 } } };
			} else {
				vertex_indices = { { { 0, 1, 2 }, { 1, 3, 2 }, { 2, 3, 0 }, { 3, 1, 0 } } };
			}
			for (std::size_t i = 0; i < 4; ++i) {
				faces[i] = result._add_face({
					vert_ids[vertex_indices[i][0]],
					vert_ids[vertex_indices[i][1]],
					vert_ids[vertex_indices[i][2]]
				});
			}
		}

		{ // initialize references
			std::array<std::array<std::pair<std::uint32_t, std::uint32_t>, 3>, 4> neighbor_indices;
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
					const auto [other_face_id, face_vert_id] = neighbor_indices[i][j];
					result._faces_pool[static_cast<face_id>(i)].edges[j] =
						half_edge_ref(faces[other_face_id], static_cast<face_vertex_ref>(face_vert_id));
				}
			}
		}

		return result;
	}

	state::~state() {
		if (_any_face != face_id::invalid) {
			face_id fid = _any_face;
			do {
				const face_id next = _faces_pool[fid].next;
				_faces_pool.free(fid);
				fid = next;
			} while (fid != _any_face);
		}
	}

	vertex_id state::add_vertex_hint(vec3 v, face_id hint) {
		const vertex_id result = _add_vertex(v);

		auto deref = [this](half_edge_ref r) -> half_edge_ref& {
			return _faces_pool[r.face].edges[std::to_underlying(r.vertex)];
		};

		half_edge_ref boundary_edge = nullptr;
		{ // find all faces that should be removed & create new faces
			auto bookmark = get_scratch_bookmark();

			auto face_marked = bookmark.create_vector_array<bool>(_faces.size(), false);
			std::stack<face_id, std::deque<face_id, memory::stack_allocator::std_allocator<face_id>>> stk(bookmark.create_std_allocator<face_id>());
			auto is_face_marked = [&](face_id i) {
				return face_marked[std::to_underlying(i)];
			};
			auto mark_face = [&](face_id i) {
				stk.emplace(i);
				face_marked[std::to_underlying(i)] = true;
			};

			mark_face(hint);
			while (!stk.empty()) {
				const face_id cur = stk.top();
				const face &cur_face = _faces_pool[cur];
				stk.pop();
				for (std::size_t i = 0; i < 3; ++i) {
					const half_edge_ref half_edge = cur_face.edges[i];
					if (!half_edge) {
						continue;
					}
					// make sure there are no more references to this face, since we're gonna delete it
					deref(half_edge) = nullptr;
					if (is_face_marked(half_edge.face)) {
						continue; // already marked for deletion
					}
					face &other_face = _faces_pool[half_edge.face];
					const vec3 offset = v - _vertices[std::to_underlying(other_face.vertex_indices[0])];
					const bool delete_face = vec::dot(other_face.normal, offset) > 0.0f;
					if (delete_face) {
						mark_face(half_edge.face);
					} else {
						// we've found a boundary edge
						boundary_edge = half_edge;
					}
				}
				_remove_face(cur);
			}
		}
		crash_if(!boundary_edge);

		// update references of all newly-created faces
		half_edge_ref ref = boundary_edge;
		half_edge_ref last_new_face = nullptr;
		half_edge_ref first_new_face = nullptr;
		do {
			face &ref_face = _faces_pool[ref.face];

			// create new face
			face_id new_face_id = _add_face({
				ref_face.get_vertex(next_in_face(ref.vertex)),
				ref_face.get_vertex(ref.vertex),
				result
			});
			face &new_face = _faces_pool[new_face_id];

			// update references
			new_face.edges[0] = ref;
			deref(ref) = half_edge_ref(new_face_id, static_cast<face_vertex_ref>(0));
			if (last_new_face) {
				new_face.edges[1] = last_new_face;
				deref(last_new_face) = half_edge_ref(new_face_id, static_cast<face_vertex_ref>(1));
			} else {
				first_new_face = half_edge_ref(new_face_id, static_cast<face_vertex_ref>(1));
			}
			last_new_face = half_edge_ref(new_face_id, static_cast<face_vertex_ref>(2));

			// update loop variable
			ref = ref.next_in_face();
			while (deref(ref) && ref != boundary_edge) {
				ref = deref(ref).next_in_face();
			}
		} while (ref != boundary_edge);
		deref(first_new_face) = last_new_face;
		deref(last_new_face) = first_new_face;
		return result;
	}

	std::optional<vertex_id> state::add_vertex(vec3 v) {
		face_id cur_face = _any_face;
		do {
			const face &f = _faces_pool[cur_face];
			const vec3 first_vert = _vertices[std::to_underlying(f.vertex_indices[0])];
			if (vec::dot(f.normal, v - first_vert) > 0.0f) {
				return add_vertex_hint(v, cur_face);
			}
			cur_face = f.next;
		} while (cur_face != _any_face);
		return std::nullopt;
	}

	vertex_id state::_add_vertex(vec3 v) {
		crash_if(_num_verts_added >= _vertices.size());
		_vertices[_num_verts_added] = v;
		const auto result = static_cast<vertex_id>(_num_verts_added);
		++_num_verts_added;
		return result;
	}

	face_id state::_add_face(std::array<vertex_id, 3> verts) {
		const vec3 normal = vec::cross(
			_vertices[std::to_underlying(verts[1])] - _vertices[std::to_underlying(verts[0])],
			_vertices[std::to_underlying(verts[2])] - _vertices[std::to_underlying(verts[0])]
		);
		auto &&[fi, f] = _faces_pool.allocate(verts, normal);

		// insert this face into the linked list
		if (_any_face == face_id::invalid) {
			_any_face = f.previous = f.next = fi;
		} else {
			face &next_face = _faces_pool[_any_face];
			face &prev_face = _faces_pool[next_face.previous];
			f.next = _any_face;
			f.previous = next_face.previous;
			next_face.previous = prev_face.next = fi;
		}

		if (on_face_added) {
			on_face_added(*this, fi);
		}

		return fi;
	}

	void state::_remove_face(face_id i) {
		if (on_face_removing) {
			on_face_removing(*this, i);
		}

		const face &f = _faces_pool[i];
		_faces_pool[f.next].previous = f.previous;
		_faces_pool[f.previous].next = f.next;
		if (_any_face == i) {
			_any_face = f.next;
		}
		_faces_pool.free(i);
	}
}
