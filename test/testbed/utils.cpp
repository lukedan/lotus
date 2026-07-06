#include "utils.h"

#include <vector>

#include <lotus/math/constants.h>
#include <lotus/renderer/context/constant_uploader.h>

#include <lotus/renderer/shader_types_include_wrapper.h>

#include "lotus/physics/world.h"

namespace shader_types {
#include "shaders/shader_types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>

void debug_render::draw_point(vec3 p, lotus::linear_rgba_f32 color, scalar size) {
	if (size <= 0.0f) {
		size = ctx->point_size;
	}
	point_vertices.emplace_back(p.into<f32>(), color, size);
}

void debug_render::draw_line(vec3 a, vec3 b, lotus::linear_rgba_f32 color) {
	const auto first_index = static_cast<u32>(line_vertices.size());
	line_vertex &v1 = line_vertices.emplace_back();
	v1.position = a.into<f32>();
	v1.color    = color;
	line_vertex &v2 = line_vertices.emplace_back();
	v2.position = b.into<f32>();
	v2.color    = color;
	line_indices.emplace_back(first_index);
	line_indices.emplace_back(first_index + 1);
}

void debug_render::draw_body(
	std::span<const vec3> positions,
	std::span<const vec3> normals,
	std::span<const u32> indices,
	mat44s transform,
	lotus::linear_rgba_f32 color,
	bool wireframe
) {
	std::vector<vec3> generated_normals;
	if (normals.empty()) {
		generated_normals.resize(positions.size(), lotus::zero);
		for (usize i = 0; i < indices.size(); i += 3) {
			auto i1 = indices[i + 0];
			auto i2 = indices[i + 1];
			auto i3 = indices[i + 2];
			auto p1 = positions[i1];
			auto p2 = positions[i2];
			auto p3 = positions[i3];
			auto diff = lotus::vec::cross(p2 - p1, p3 - p1);
			generated_normals[i1] += diff;
			generated_normals[i2] += diff;
			generated_normals[i3] += diff;
		}
		normals = generated_normals;
	}

	u32 first_vert;
	if (wireframe) {
		first_vert = static_cast<u32>(line_vertices.size());
		for (usize i = 0; i < positions.size(); ++i) {
			line_vertex &vert = line_vertices.emplace_back();
			vert.position = (transform * vec4(positions[i], 1.0f)).subvector<3>(0).into<f32>();
			vert.color    = color;
		}
	} else {
		const mat33s normal_transform = transform.block<3, 3>(0, 0).inverse().transposed();
		first_vert = static_cast<u32>(mesh_vertices.size());
		for (usize i = 0; i < positions.size(); ++i) {
			vertex &vert = mesh_vertices.emplace_back();
			vert.position    = (transform * vec4(positions[i], 1.0f)).subvector<3>(0).into<f32>();
			vert.position_ls = positions[i].into<f32>();
			vert.color       = color;
			vert.normal      = lotus::vecu::normalize(normal_transform * normals[i]).into<f32>();
		}
	}
	for (usize i = 0; i < indices.size(); i += 3) {
		if (wireframe) {
			for (usize j = 0; j < 3; ++j) {
				auto cur = indices[i + j];
				auto next = indices[i + (j + 1) % 3];
				if (cur < next) {
					line_indices.emplace_back(cur + first_vert);
					line_indices.emplace_back(next + first_vert);
				}
			}
		} else {
			for (usize j = 0; j < 3; ++j) {
				mesh_indices.emplace_back(indices[i + j] + first_vert);
			}
		}
	}
}

void debug_render::draw_sphere(mat44s transform, lotus::linear_rgba_f32 color, bool wireframe) {
	constexpr u32 _z_slices = 10;
	constexpr u32 _xy_slices = 30;
	constexpr scalar _z_slice_angle = lotus::physics::pi / _z_slices;
	constexpr scalar _xy_slice_angle = 2.0 * lotus::physics::pi / _xy_slices;

	static std::vector<vec3> _vertices;
	static std::vector<u32> _indices;

	if (_vertices.empty()) {
		// http://www.songho.ca/opengl/gl_sphere.html
		for (u32 i = 0; i <= _z_slices; ++i) {
			const scalar z_angle = lotus::physics::pi / 2 - static_cast<scalar>(i) * _z_slice_angle;
			const scalar xy = 0.5f * std::cos(z_angle);
			const scalar z = 0.5f * std::sin(z_angle);

			for (u32 j = 0; j <= _xy_slices; ++j) {
				const scalar xy_angle = static_cast<scalar>(j) * _xy_slice_angle;

				const scalar x = xy * std::cos(xy_angle);
				const scalar y = xy * std::sin(xy_angle);
				_vertices.emplace_back(x, y, z);
			}
		}

		for (u32 i = 0; i < _z_slices; ++i) {
			u32 k1 = i * (_xy_slices + 1);
			u32 k2 = k1 + _xy_slices + 1;

			for (u32 j = 0; j < _xy_slices; ++j, ++k1, ++k2) {
				if (i != 0) {
					_indices.emplace_back(k1);
					_indices.emplace_back(k2);
					_indices.emplace_back(k1 + 1);
				}
				if (i != (_z_slices - 1)) {
					_indices.emplace_back(k1 + 1);
					_indices.emplace_back(k2);
					_indices.emplace_back(k2 + 1);
				}
			}
		}
	}

	draw_body(_vertices, _vertices, _indices, transform, color, wireframe);
}

void debug_render::draw_box(mat44s transform, lotus::linear_rgba_f32 color, bool wireframe) {
	static constexpr std::array _vertices = {
		// XY positive
		vec3(-0.5f, -0.5f,  0.5f),
		vec3( 0.5f, -0.5f,  0.5f),
		vec3(-0.5f,  0.5f,  0.5f),
		vec3( 0.5f,  0.5f,  0.5f),
		// XY negative
		vec3(-0.5f, -0.5f, -0.5f),
		vec3( 0.5f, -0.5f, -0.5f),
		vec3(-0.5f,  0.5f, -0.5f),
		vec3( 0.5f,  0.5f, -0.5f),
		// XZ negative
		vec3(-0.5f, -0.5f, -0.5f),
		vec3( 0.5f, -0.5f, -0.5f),
		vec3(-0.5f, -0.5f,  0.5f),
		vec3( 0.5f, -0.5f,  0.5f),
		// XZ positive
		vec3(-0.5f,  0.5f, -0.5f),
		vec3( 0.5f,  0.5f, -0.5f),
		vec3(-0.5f,  0.5f,  0.5f),
		vec3( 0.5f,  0.5f,  0.5f),
		// YZ positive
		vec3( 0.5f, -0.5f, -0.5f),
		vec3( 0.5f,  0.5f, -0.5f),
		vec3( 0.5f, -0.5f,  0.5f),
		vec3( 0.5f,  0.5f,  0.5f),
		// YZ negative
		vec3(-0.5f, -0.5f, -0.5f),
		vec3(-0.5f,  0.5f, -0.5f),
		vec3(-0.5f, -0.5f,  0.5f),
		vec3(-0.5f,  0.5f,  0.5f),
	};
	static constexpr std::array _normals = {
		vec3( 0.0f,  0.0f,  1.0f), vec3( 0.0f,  0.0f,  1.0f), vec3( 0.0f,  0.0f,  1.0f), vec3( 0.0f,  0.0f,  1.0f),
		vec3( 0.0f,  0.0f, -1.0f), vec3( 0.0f,  0.0f, -1.0f), vec3( 0.0f,  0.0f, -1.0f), vec3( 0.0f,  0.0f, -1.0f),
		vec3( 0.0f, -1.0f,  0.0f), vec3( 0.0f, -1.0f,  0.0f), vec3( 0.0f, -1.0f,  0.0f), vec3( 0.0f, -1.0f,  0.0f),
		vec3( 0.0f,  1.0f,  0.0f), vec3( 0.0f,  1.0f,  0.0f), vec3( 0.0f,  1.0f,  0.0f), vec3( 0.0f,  1.0f,  0.0f),
		vec3( 1.0f,  0.0f,  0.0f), vec3( 1.0f,  0.0f,  0.0f), vec3( 1.0f,  0.0f,  0.0f), vec3( 1.0f,  0.0f,  0.0f),
		vec3(-1.0f,  0.0f,  0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(-1.0f,  0.0f,  0.0f),
	};
	static constexpr std::array<u32, 36> _indices = {
		0  + 0, 0  + 1, 0  + 2, 0  + 1, 0  + 3, 0  + 2,
		4  + 0, 4  + 2, 4  + 1, 4  + 1, 4  + 2, 4  + 3,
		8  + 0, 8  + 1, 8  + 2, 8  + 1, 8  + 3, 8  + 2,
		12 + 0, 12 + 2, 12 + 1, 12 + 1, 12 + 2, 12 + 3,
		16 + 0, 16 + 1, 16 + 2, 16 + 1, 16 + 3, 16 + 2,
		20 + 0, 20 + 2, 20 + 1, 20 + 1, 20 + 2, 20 + 3,
	};

	draw_body(_vertices, _normals, _indices, transform, color, wireframe);
}

void debug_render::draw_frame(uquats ori, vec3 pos, scalar size) {
	const vec3 x = ori.rotate(vec3(size, 0.0f, 0.0f));
	const vec3 y = ori.rotate(vec3(0.0f, size, 0.0f));
	const vec3 z = ori.rotate(vec3(0.0f, 0.0f, size));

	draw_line(pos, pos + x, lotus::linear_rgba_f32(1.0f, 0.0f, 0.0f, 1.0f));
	draw_line(pos, pos + y, lotus::linear_rgba_f32(0.0f, 1.0f, 0.0f, 1.0f));
	draw_line(pos, pos + z, lotus::linear_rgba_f32(0.0f, 0.0f, 1.0f, 1.0f));
}

void debug_render::draw_physics_body(const lotus::collision::shapes::plane&, mat44s transform, const body_visual *visual, bool wireframe) {
	const i32 size = 100;
	const vec3 vx = { 1.0f, 0.0f, 0.0f };
	const vec3 vy = { 0.0f, 1.0f, 0.0f };
	lotus::linear_rgba_f32 color = visual ? visual->color : lotus::linear_rgba_f32(1.0f, 1.0f, 1.0f, 1.0f);
	if (wireframe) {
		for (int x = -size; x <= size; ++x) {
			draw_line(
				vx * static_cast<f32>(x) + vy * -static_cast<f32>(size),
				vx * static_cast<f32>(x) + vy * static_cast<f32>(size),
				transform,
				color
			);
			draw_line(
				vy * static_cast<f32>(x) + vx * -static_cast<f32>(size),
				vy * static_cast<f32>(x) + vx * static_cast<f32>(size),
				transform,
				color
			);
		}
	} else {
		const scalar scale_ls = 10.0f;
		const scalar scale_ws = 10.0f;
		const vec3 positions[] = {
			vx *  scale_ls + vy *  scale_ls,
			vx * -scale_ls + vy *  scale_ls,
			vx *  scale_ls + vy * -scale_ls,
			vx * -scale_ls + vy * -scale_ls,
		};
		const u32 indices[] = {
			0, 1, 3, 0, 3, 2
		};
		draw_body(positions, {}, indices, transform * mat44s::diagonal(scale_ws, scale_ws, scale_ws, 1.0f), color, false);
	}
}

void debug_render::draw_physics_body(const lotus::collision::shapes::sphere &sphere, mat44s transform, const body_visual *visual, bool wireframe) {
	auto mat = mat44s::identity();
	mat.set_block(0, 0, mat33s::identity() * 2.0f * sphere.radius);
	mat.set_block(0, 3, sphere.offset);
	draw_sphere(transform * mat, visual ? visual->color : lotus::linear_rgba_f32(1.0f, 1.0f, 1.0f, 1.0f), wireframe);
}

void debug_render::draw_physics_body(const lotus::collision::shapes::convex_polyhedron &poly, mat44s transform, const body_visual *visual, bool wireframe) {
	if (visual) {
		draw_body(poly.vertices, {}, visual->triangles, transform, visual->color, wireframe);
	} else {
		std::vector<vec3> verts;
		std::vector<u32> indices;

		using convex_hull = lotus::incremental_convex_hull;

		auto storage = convex_hull::create_storage_for_num_vertices(static_cast<u32>(poly.vertices.size()));
		auto hull = storage.create_state_for_tetrahedron({ poly.vertices[0], poly.vertices[1], poly.vertices[2], poly.vertices[3] });
		for (usize i = 4; i < poly.vertices.size(); ++i) {
			hull.add_vertex(poly.vertices[i]);
		}

		{
			convex_hull::face_id fi = hull.get_any_face();
			do {
				const convex_hull::face &face = hull.get_face(fi);
				const auto i0 = static_cast<u32>(verts.size());
				const vec3 v1 = hull.get_vertex(face.vertex_indices[0]);
				const vec3 v2 = hull.get_vertex(face.vertex_indices[1]);
				const vec3 v3 = hull.get_vertex(face.vertex_indices[2]);
				verts.emplace_back(v1);
				verts.emplace_back(v2);
				verts.emplace_back(v3);
				indices.emplace_back(i0);
				indices.emplace_back(i0 + 1);
				indices.emplace_back(i0 + 2);
				fi = face.next;
			} while (fi != hull.get_any_face());
		}

		draw_body(verts, {}, indices, transform, lotus::linear_rgba_f32(1.0f, 1.0f, 1.0f, 1.0f), wireframe);

		if (ctx->draw_faces) {
			// draw faces
			for (const lotus::collision::shapes::convex_polyhedron::face &face : poly.faces) {
				const vec3 offset = face.normal * 0.01f;
				vec3 last_vert = (transform * vec4(poly.get_vertex(face.vertex_indices.back()) + offset, 1.0f)).subvector<3>(0);
				for (usize i = 0; i < face.vertex_indices.size(); ++i) {
					const vec3 vert = (transform * vec4(poly.get_vertex(face.vertex_indices[i]) + offset, 1.0f)).subvector<3>(0);
					draw_line(
						last_vert,
						vert,
						lotus::linear_rgba_f32(0.0f, static_cast<f32>(i) / static_cast<f32>(face.vertex_indices.size() - 1), 0.0f, 1.0f)
					);
					last_vert = vert;
				}
			}
		}
	}
}

void debug_render::draw_world(const lotus::physics::world &world) {
	for (const lotus::physics::body *b : world.get_bodies()) {
		const body_visual *visual = nullptr;
		if (b->user_data) {
			visual = static_cast<const body_visual*>(b->user_data);
		}

		auto mat = mat44s::identity();
		mat.set_block(0, 0, b->state.position.orientation.into_rotation_matrix());
		mat.set_block(0, 3, b->state.position.position);

		std::visit(
			[&](const auto &shape) {
				draw_physics_body(shape, mat, visual, ctx->wireframe_bodies);
			},
			b->body_shape->value
		);
	}

	for (const auto &c : world.contacts) {
		for (const auto &cp : c.contact_points) {
			const vec3 p1 = c.body1->state.position.local_to_global(cp.local_position1);
			const vec3 p2 = c.body2->state.position.local_to_global(cp.local_position2);
			if (ctx->draw_contact_points) {
				draw_point(p1, lotus::linear_rgba_f32(1.0f, 0.0f, 0.0f, 1.0f));
				draw_point(p2, lotus::linear_rgba_f32(0.0f, 1.0f, 0.0f, 1.0f));
			}
			if (ctx->draw_contact_normals) {
				draw_line(p1, p1 + c.tangents.normal * 0.1f, lotus::linear_rgba_f32(0.0f, 0.0f, 1.0f, 1.0f));
			}
			if (ctx->draw_contact_relationships) {
				draw_line(p1, p2, lotus::linear_rgba_f32(1.0f, 0.0f, 1.0f, 1.0f));
				draw_line(p1, c.body1->state.position.position, lotus::linear_rgba_f32(1.0f, 0.0f, 1.0f, 1.0f));
				draw_line(p2, c.body2->state.position.position, lotus::linear_rgba_f32(1.0f, 0.0f, 1.0f, 1.0f));
			}
		}
	}

	for (const lotus::physics::constraints::spring &spring : world.springs) {
		const vec3 p1 = spring.get_global_position1();
		const vec3 p2 = spring.get_global_position2();
		const scalar len_percentage = (p1 - p2).norm() / spring.initial_length;
		draw_line(
			p1,
			p2,
			lotus::linear_rgba_f32::from_vec4((
				len_percentage < 1.0f ?
				lotus::mat::lerp(vec4(1.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f), len_percentage) :
				lotus::mat::lerp(vec4(0.0f, 0.0f, 1.0f, 1.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f), 2.0f - len_percentage)
			).into<f32>())
		);
	}

	for (const lotus::physics::constraints::pin &pin : world.pins) {
		const vec3 p1 = pin.get_global_position1();
		const vec3 p2 = pin.get_global_position2();
		if (pin.body1) {
			draw_line(p1, pin.body1->state.position.position, lotus::linear_rgba_f32(1.0f, 1.0f, 0.0f, 1.0f));
		}
		if (pin.body2) {
			draw_line(p2, pin.body2->state.position.position, lotus::linear_rgba_f32(1.0f, 1.0f, 0.0f, 1.0f));
		}
		draw_line(p1, p2, lotus::linear_rgba_f32(1.0f, 1.0f, 0.0f, 1.0f));
		draw_point(p1, lotus::linear_rgba_f32(1.0f, 0.0f, 0.0f, 1.0f));
		draw_point(p2, lotus::linear_rgba_f32(0.0f, 1.0f, 0.0f, 1.0f));
	}

	// debug stuff
	if (ctx->draw_body_velocities) {
		for (const lotus::physics::body *b : world.get_bodies()) {
			draw_line(b->state.position.position, b->state.position.position + b->state.velocity.linear, lotus::linear_rgba_f32(1.0f, 0.0f, 0.0f, 1.0f));
			draw_line(b->state.position.position, b->state.position.position + b->state.velocity.angular, lotus::linear_rgba_f32(0.0f, 1.0f, 0.0f, 1.0f));
		}
	}
}


void debug_render::draw_system(const lotus::physics::solvers::avbd::solver &solver) {
	draw_world(*solver.physics_world);

	if (ctx->draw_particles) {
		for (const lotus::physics::particle &p : solver.particles) {
			draw_point(p.state.position, lotus::linear_rgba_f32(0.0f, 1.0f, 1.0f, 1.0f));
		}
	}

	if (ctx->draw_orientations) {
		for (const lotus::physics::solvers::avbd::constraints::cosserat_rod::stretch_shear &c : solver.rod_stretch_shear_constraints) {
			const vec3 p1 = solver.particles[c.particle1].state.position;
			const vec3 p2 = solver.particles[c.particle2].state.position;
			draw_frame(
				solver.orientations[c.orientation].state.orientation,
				0.5f * (p1 + p2),
				(p1 - p2).norm() * 0.3f
			);
		}
	}


	// segments
	for (const lotus::physics::solvers::avbd::constraints::cosserat_rod::stretch_shear &constraint : solver.rod_stretch_shear_constraints) {
		draw_line(solver.particles[constraint.particle1].state.position, solver.particles[constraint.particle2].state.position, lotus::linear_rgba_f32(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

void debug_render::draw_system(const lotus::physics::solvers::sequential_impulse::solver &solver) {
	draw_world(*solver.physics_world);
}

void debug_render::draw_system(const lotus::physics::solvers::xpbd::solver &solver) {
	draw_world(*solver.physics_world);

	if (ctx->draw_particles) {
		for (const lotus::physics::particle &p : solver.particles) {
			draw_point(p.state.position, lotus::linear_rgba_f32(0.0f, 1.0f, 1.0f, 1.0f));
		}
	}

	if (ctx->draw_orientations) {
		for (const lotus::physics::solvers::xpbd::constraints::cosserat_rod::stretch_shear &c : solver.rod_stretch_shear_constraints) {
			const vec3 p1 = solver.particles[c.particle1].state.position;
			const vec3 p2 = solver.particles[c.particle2].state.position;
			draw_frame(
				solver.orientations[c.orientation].state.orientation,
				0.5f * (p1 + p2),
				(p1 - p2).norm() * 0.3f
			);
		}
	}

	// surfaces
	std::vector<vec3> positions;
	if (!ctx->wireframe_surfaces) {
		for (const auto &p : solver.particles) {
			positions.emplace_back(p.state.position);
		}
	}
	for (const auto &surface : surfaces) {
		if (ctx->wireframe_surfaces) {
			for (usize i = 0; i < surface.triangles.size(); i += 3) {
				auto p1 = solver.particles[surface.triangles[i]].state.position;
				auto p2 = solver.particles[surface.triangles[i + 1]].state.position;
				auto p3 = solver.particles[surface.triangles[i + 2]].state.position;
				draw_line(p1, p2, surface.color);
				draw_line(p2, p3, surface.color);
				draw_line(p3, p1, surface.color);
			}
		} else {
			draw_body(positions, {}, surface.triangles, mat44s::identity(), surface.color, false);
		}
	}

	// segments
	for (const lotus::physics::solvers::xpbd::constraints::cosserat_rod::stretch_shear &constraint : solver.rod_stretch_shear_constraints) {
		draw_line(solver.particles[constraint.particle1].state.position, solver.particles[constraint.particle2].state.position, lotus::linear_rgba_f32(1.0f, 1.0f, 1.0f, 1.0f));
	}

	// debug stuff
	for (const auto &c : solver.contact_constraints) {
		auto p1 = c.body1->state.position.local_to_global(c.offset1);
		auto p2 = c.body2->state.position.local_to_global(c.offset2);
		if (ctx->draw_contact_points) {
			draw_point(p1, lotus::linear_rgba_f32(0.0f, 0.0f, 1.0f, 1.0f));
			draw_point(p2, lotus::linear_rgba_f32(0.0f, 0.0f, 1.0f, 1.0f));
		}
	}
}

void debug_render::flush(
	lotus::renderer::context &rctx, lotus::renderer::context::queue &q, lotus::renderer::constant_uploader &uploader,
	lotus::renderer::recorded_resources::swap_chain swap_chain, cvec2u32 size
) {
	auto upload_data_buffer_bytes = [&](lotus::renderer::context::queue &q, lotus::gpu::buffer_usage_mask usages, std::span<const std::byte> data, std::u8string_view name) -> lotus::renderer::buffer {
		if (data.empty()) {
			return nullptr;
		}
		auto result = rctx.request_buffer(name, data.size_bytes(), usages | lotus::gpu::buffer_usage_mask::copy_destination, ctx->resource_pool);
		auto temp = rctx.request_buffer(name, data.size_bytes(), lotus::gpu::buffer_usage_mask::copy_source, ctx->upload_pool);
		rctx.write_data_to_buffer(temp, data);
		q.copy_buffer(temp, result, 0, 0, data.size_bytes(), u8"Upload data buffer");
		return result;
	};
	auto upload_data_buffer_vec = [&]<typename T>(lotus::renderer::context::queue &q, lotus::gpu::buffer_usage_mask usages, const std::vector<T> &data, std::u8string_view name) -> lotus::renderer::buffer {
		return upload_data_buffer_bytes(q, usages, { reinterpret_cast<const std::byte*>(data.data()), sizeof(T) * data.size() }, name);
	};

	auto mesh_vert_buf = upload_data_buffer_vec(q, lotus::gpu::buffer_usage_mask::vertex_buffer, mesh_vertices, u8"Mesh Vertices");
	auto mesh_idx_buf = upload_data_buffer_vec(q, lotus::gpu::buffer_usage_mask::index_buffer, mesh_indices, u8"Mesh Indices");

	auto line_vert_buf = upload_data_buffer_vec(q, lotus::gpu::buffer_usage_mask::vertex_buffer, line_vertices, u8"Line Vertices");
	auto line_idx_buf = upload_data_buffer_vec(q, lotus::gpu::buffer_usage_mask::index_buffer, line_indices, u8"Line Indices");

	auto point_buf = upload_data_buffer_vec(q, lotus::gpu::buffer_usage_mask::vertex_buffer, point_vertices, u8"Point Vertices");


	lotus::renderer::image2d_view color_tex = rctx.request_image2d(
		u8"Color", size, 1, lotus::gpu::format::r8g8b8a8_unorm, lotus::gpu::image_usage_mask::color_render_target | lotus::gpu::image_usage_mask::shader_read, ctx->resource_pool
	);
	lotus::renderer::image2d_view normal_tex = rctx.request_image2d(
		u8"Normal", size, 1, lotus::gpu::format::r8g8b8a8_snorm, lotus::gpu::image_usage_mask::color_render_target | lotus::gpu::image_usage_mask::shader_read, ctx->resource_pool
	);
	lotus::renderer::image2d_view ssao_tex = rctx.request_image2d(
		u8"SSAO", size, 1, lotus::gpu::format::r8_unorm, lotus::gpu::image_usage_mask::shader_write | lotus::gpu::image_usage_mask::shader_read, ctx->resource_pool
	);
	lotus::renderer::image2d_view depth_stencil = rctx.request_image2d(
		u8"Depth Stencil", size, 1, lotus::gpu::format::d32_float_s8, lotus::gpu::image_usage_mask::depth_stencil_render_target | lotus::gpu::image_usage_mask::shader_read, ctx->resource_pool
	);

	const vec3 sky_color = vec3(0.5f, 0.5f, 1.0f);

	const vec3 main_light_dir = lotus::vecu::normalize(vec3(0.3f, 0.4f, 0.5f));
	const vec3 fill_light_dir = lotus::zero;
	const vec3 main_light_color = vec3::filled(0.9f);
	const vec3 fill_light_color = sky_color * 0.4f;

	const auto draw_shadow_stencil = [&]() {
		// extrude and render shadow faces
		const auto num_verts = static_cast<u32>(mesh_vertices.size());
		std::vector<cvec3f32> shadow_mesh_verts;
		std::vector<u32> shadow_mesh_indices;
		for (const vertex &v : mesh_vertices) {
			shadow_mesh_verts.emplace_back(v.position);
		}
		for (u32 i = 0; i < num_verts; ++i) {
			const scalar extrude_factor = 100.0f;
			shadow_mesh_verts.emplace_back((shadow_mesh_verts[i] - main_light_dir * extrude_factor).into<f32>());
		}
		for (usize i = 0; i < mesh_indices.size(); i += 3) {
			u32 i0 = mesh_indices[i];
			u32 i1 = mesh_indices[i + 1];
			u32 i2 = mesh_indices[i + 2];
			cvec3f32 p0 = shadow_mesh_verts[i0];
			cvec3f32 p1 = shadow_mesh_verts[i1];
			cvec3f32 p2 = shadow_mesh_verts[i2];
			if (lotus::vec::dot(lotus::vec::cross(p1 - p0, p2 - p0), main_light_dir) > 0.0f) {
				continue;
			}
			const u32 j0 = i0 + num_verts;
			const u32 j1 = i1 + num_verts;
			const u32 j2 = i2 + num_verts;
			shadow_mesh_indices.insert(shadow_mesh_indices.end(), { j0, j1, j2 });
			shadow_mesh_indices.insert(shadow_mesh_indices.end(), { i0, i2, i1 });

			shadow_mesh_indices.insert(shadow_mesh_indices.end(), { i0, i1, j1 });
			shadow_mesh_indices.insert(shadow_mesh_indices.end(), { i0, j1, j0 });

			shadow_mesh_indices.insert(shadow_mesh_indices.end(), { i1, i2, j2 });
			shadow_mesh_indices.insert(shadow_mesh_indices.end(), { i1, j2, j1 });

			shadow_mesh_indices.insert(shadow_mesh_indices.end(), { i2, i0, j0 });
			shadow_mesh_indices.insert(shadow_mesh_indices.end(), { i2, j0, j2 });
		}

		auto shadow_vert_buf = upload_data_buffer_vec(q, lotus::gpu::buffer_usage_mask::vertex_buffer, shadow_mesh_verts, u8"Shadow Vertices");
		auto shadow_idx_buf = upload_data_buffer_vec(q, lotus::gpu::buffer_usage_mask::index_buffer, shadow_mesh_indices, u8"Shadow Indices");

		auto vertex_input_elements = {
			lotus::gpu::input_buffer_element(u8"POSITION", 0, lotus::gpu::format::r32g32b32_float, 0),
		};
		auto pipeline_state = lotus::renderer::graphics_pipeline_state(
			{ lotus::gpu::render_target_blend_options::disabled(), },
			lotus::gpu::rasterizer_options(lotus::gpu::depth_bias_options::disabled(), lotus::gpu::front_facing_mode::clockwise, lotus::gpu::cull_mode::none, false),
			lotus::gpu::depth_stencil_options(
				true, false, lotus::gpu::comparison_function::less, true, 0xFF, 0xFF,
				lotus::gpu::stencil_options::create(lotus::gpu::comparison_function::always, lotus::gpu::stencil_operation::keep, lotus::gpu::stencil_operation::keep, lotus::gpu::stencil_operation::increment_and_wrap),
				lotus::gpu::stencil_options::create(lotus::gpu::comparison_function::always, lotus::gpu::stencil_operation::keep, lotus::gpu::stencil_operation::keep, lotus::gpu::stencil_operation::decrement_and_wrap)
			)
		);
		shader_types::shadow_constants constants;
		constants.projection_view = ctx->camera.projection_view_matrix.into<f32>();

		auto pass = q.begin_pass({}, lotus::renderer::image2d_depth_stencil(
			depth_stencil, lotus::gpu::depth_render_target_access::create_preserve_and_write(), lotus::gpu::stencil_render_target_access::create_preserve_and_write()
		), size, u8"Shadow Stencil");
		if (!shadow_mesh_indices.empty()) {
			pass.draw_instanced(
				{ lotus::renderer::input_buffer_binding(0, shadow_vert_buf, 0, sizeof(cvec3f32), lotus::gpu::input_buffer_rate::per_vertex, vertex_input_elements) },
				static_cast<u32>(shadow_mesh_verts.size()),
				lotus::renderer::index_buffer_binding(shadow_idx_buf, 0, lotus::gpu::index_format::uint32),
				static_cast<u32>(shadow_mesh_indices.size()),
				lotus::gpu::primitive_topology::triangle_list,
				lotus::renderer::all_resource_bindings({}, {
					{ u8"constants", uploader.upload(constants) }
				}),
				ctx->shadow_vs, nullptr,
				pipeline_state,
				1,
				u8"Shadow Meshes"
			);
		}
	};

	const auto draw_light_quad = [&](vec3 light_dir, vec3 light_color, bool has_shadows) {
		auto stencil_state =
			has_shadows ?
			lotus::gpu::stencil_options::create(lotus::gpu::comparison_function::equal, lotus::gpu::stencil_operation::keep, lotus::gpu::stencil_operation::keep, lotus::gpu::stencil_operation::keep) :
			lotus::gpu::stencil_options::always_pass_no_op();
		auto pipeline_state = lotus::renderer::graphics_pipeline_state(
			{
				lotus::gpu::render_target_blend_options::create_custom(
					lotus::gpu::blend_factor::one, lotus::gpu::blend_factor::one, lotus::gpu::blend_operation::add,
					lotus::gpu::blend_factor::one, lotus::gpu::blend_factor::zero, lotus::gpu::blend_operation::max,
					lotus::gpu::channel_mask::all
				),
			},
			lotus::gpu::rasterizer_options(lotus::gpu::depth_bias_options::disabled(), lotus::gpu::front_facing_mode::clockwise, lotus::gpu::cull_mode::none, false),
			lotus::gpu::depth_stencil_options(
				true, false, lotus::gpu::comparison_function::not_equal, true, 0xFF, 0, stencil_state, stencil_state
			)
		);
		shader_types::light_constants constants;
		constants.light_direction = light_dir.into<f32>();
		constants.light_color     = light_color.into<f32>();

		auto pass = q.begin_pass(
			{ lotus::renderer::image2d_color(swap_chain, lotus::gpu::color_render_target_access::create_preserve_and_write()) },
			lotus::renderer::image2d_depth_stencil(depth_stencil, lotus::gpu::depth_render_target_access::create_preserve_and_write(), lotus::gpu::stencil_render_target_access::create_preserve_and_write()),
			size,
			u8"Light Quad"
		);
		pass.set_stencil_reference(0, u8"Light Stencil");
		pass.draw_instanced(
			{}, 3,
			nullptr, 0,
			lotus::gpu::primitive_topology::triangle_list,
			{ {}, {
				{ u8"constants",  uploader.upload(constants)     },
				{ u8"color_tex",  color_tex.bind_as_read_only()  },
				{ u8"normal_tex", normal_tex.bind_as_read_only() },
				{ u8"ssao",       ssao_tex.bind_as_read_only()   },
			}},
			ctx->fullscreen_quad_vs,
			ctx->light_quad_ps,
			pipeline_state,
			1,
			u8"Light Quad"
		);
	};

	{ // clear
		auto pass = q.begin_pass(
			{ lotus::renderer::image2d_color(swap_chain, lotus::gpu::color_render_target_access::create_clear(lotus::zero)) },
			lotus::renderer::image2d_depth_stencil(depth_stencil, lotus::gpu::depth_render_target_access::create_clear(0.0f), lotus::gpu::stencil_render_target_access::create_clear(0)),
			size, u8"Clear"
		);
	}

	{ // g-buffer pass
		auto vertex_input_elements = {
			lotus::gpu::input_buffer_element(u8"POSITION", 0, lotus::gpu::format::r32g32b32_float, offsetof(debug_render::vertex, position)),
			lotus::gpu::input_buffer_element(u8"POSITION_LS", 0, lotus::gpu::format::r32g32b32_float, offsetof(debug_render::vertex, position_ls)),
			lotus::gpu::input_buffer_element(u8"COLOR", 0, lotus::gpu::format::r32g32b32a32_float, offsetof(debug_render::vertex, color)),
			lotus::gpu::input_buffer_element(u8"NORMAL", 0, lotus::gpu::format::r32g32b32_float, offsetof(debug_render::vertex, normal)),
		};
		auto pipeline_state = lotus::renderer::graphics_pipeline_state(
			{ lotus::gpu::render_target_blend_options::disabled(), lotus::gpu::render_target_blend_options::disabled(), },
			lotus::gpu::rasterizer_options(lotus::gpu::depth_bias_options::disabled(), lotus::gpu::front_facing_mode::counter_clockwise, lotus::gpu::cull_mode::cull_back, false),
			lotus::gpu::depth_stencil_options(true, true, lotus::gpu::comparison_function::greater, false, 0, 0, lotus::gpu::stencil_options::always_pass_no_op(), lotus::gpu::stencil_options::always_pass_no_op())
		);
		shader_types::default_shader_constants constants;
		constants.projection_view = ctx->camera.projection_view_matrix.into<f32>();

		auto pass = q.begin_pass(
			{
				lotus::renderer::image2d_color(color_tex, lotus::gpu::color_render_target_access::create_clear(lotus::zero)),
				lotus::renderer::image2d_color(normal_tex, lotus::gpu::color_render_target_access::create_clear(lotus::zero)),
			},
			lotus::renderer::image2d_depth_stencil(depth_stencil, lotus::gpu::depth_render_target_access::create_preserve_and_write(), lotus::gpu::stencil_render_target_access::create_preserve_and_write()),
			size,
			u8"Debug Render"
		);
		if (!mesh_vertices.empty()) {
			pass.draw_instanced(
				{
					lotus::renderer::input_buffer_binding(0, mesh_vert_buf, 0, sizeof(vertex), lotus::gpu::input_buffer_rate::per_vertex, vertex_input_elements),
				},
				static_cast<u32>(mesh_vertices.size()),
				lotus::renderer::index_buffer_binding(mesh_idx_buf, 0, lotus::gpu::index_format::uint32),
				static_cast<u32>(mesh_indices.size()),
				lotus::gpu::primitive_topology::triangle_list,
				lotus::renderer::all_resource_bindings({}, {
					{ u8"constants", uploader.upload(constants) },
				}),
				ctx->gbuffer_shader_vs, ctx->gbuffer_shader_ps,
				pipeline_state,
				1,
				u8"Debug Meshes"
			);
		}
	}

	{
		const float radius_ws = 5.0f;
		const u32 angular_samples = 3;
		const u32 radial_samples = 6;
		shader_types::ssao_constants ssao_constants;
		ssao_constants.inv_projection          = ctx->camera.projection_matrix.inverse().into<f32>();
		ssao_constants.image_size              = size;
		ssao_constants.rcp_image_size          = lotus::matm::reciprocal(size.into<f32>());
		ssao_constants.angular_samples         = angular_samples;
		ssao_constants.radial_samples          = radial_samples;
		ssao_constants.depth_linearize_mul_add = ctx->camera.depth_linearization_constants.into<f32>();
		ssao_constants.radius_pixels_1m        = 0.5f * static_cast<f32>(size[1]) * radius_ws / std::tan(0.5f * ctx->camera_params.fov_y_radians);
		ssao_constants.radius_ws               = radius_ws;
		ssao_constants.smoothing               = ctx->ssao_smoothing;
		q.run_compute_shader_with_thread_dimensions(
			ctx->ssao_cs,
			cvec3u32(size, 1u),
			{
				{
					{ 1, ctx->asset_manager->get_samplers() }
				}, {
					{ u8"constants",  uploader.upload(ssao_constants) },
					{ u8"depth_tex",  lotus::renderer::descriptor_resource::image2d(depth_stencil, lotus::renderer::image_binding_type::read_only) },
					{ u8"output",     ssao_tex.bind_as_read_write() },
				}
			},
			u8"SSAO"
		);
	}

	if (!mesh_vertices.empty()) {
		if (ctx->draw_shadows) {
			draw_shadow_stencil();
		}
		draw_light_quad(main_light_dir, main_light_color, ctx->draw_shadows);
		draw_light_quad(fill_light_dir, fill_light_color, false);
	}

	{ // sky
		auto pipeline_state = lotus::renderer::graphics_pipeline_state(
			{ lotus::gpu::render_target_blend_options::disabled(), },
			lotus::gpu::rasterizer_options(lotus::gpu::depth_bias_options::disabled(), lotus::gpu::front_facing_mode::clockwise, lotus::gpu::cull_mode::none, false),
			lotus::gpu::depth_stencil_options(
				true, false, lotus::gpu::comparison_function::equal, false, 0, 0, lotus::gpu::stencil_options::always_pass_no_op(), lotus::gpu::stencil_options::always_pass_no_op()
			)
		);
		shader_types::sky_constants constants;
		constants.color = sky_color.into<f32>();

		auto pass = q.begin_pass(
			{ lotus::renderer::image2d_color(swap_chain, lotus::gpu::color_render_target_access::create_preserve_and_write()) },
			lotus::renderer::image2d_depth_stencil(depth_stencil, lotus::gpu::depth_render_target_access::create_preserve_and_write(), lotus::gpu::stencil_render_target_access::create_preserve_and_write()),
			size,
			u8"Sky"
		);
		pass.draw_instanced(
			{}, 3,
			nullptr, 0,
			lotus::gpu::primitive_topology::triangle_list,
			{ {}, {
				{ u8"constants",  uploader.upload(constants)     },
			}},
			ctx->fullscreen_quad_vs,
			ctx->sky_ps,
			pipeline_state,
			1,
			u8"Sky"
		);
	}

	{ // points and lines
		auto point_input_elements = {
			lotus::gpu::input_buffer_element(u8"POSITION", 0, lotus::gpu::format::r32g32b32_float, offsetof(debug_render::point_vertex, position)),
			lotus::gpu::input_buffer_element(u8"COLOR", 0, lotus::gpu::format::r32g32b32a32_float, offsetof(debug_render::point_vertex, color)),
			lotus::gpu::input_buffer_element(u8"SIZE", 0, lotus::gpu::format::r32_float, offsetof(debug_render::point_vertex, size)),
		};
		auto line_input_elements = {
			lotus::gpu::input_buffer_element(u8"POSITION", 0, lotus::gpu::format::r32g32b32_float, offsetof(debug_render::line_vertex, position)),
			lotus::gpu::input_buffer_element(u8"COLOR", 0, lotus::gpu::format::r32g32b32a32_float, offsetof(debug_render::line_vertex, color)),
		};
		auto pipeline_state_blend = lotus::renderer::graphics_pipeline_state(
			{ lotus::gpu::render_target_blend_options::create_default_alpha_blend(), },
			lotus::gpu::rasterizer_options(lotus::gpu::depth_bias_options::disabled(), lotus::gpu::front_facing_mode::counter_clockwise, lotus::gpu::cull_mode::cull_back, false),
			lotus::gpu::depth_stencil_options(true, false, lotus::gpu::comparison_function::greater, false, 0, 0, lotus::gpu::stencil_options::always_pass_no_op(), lotus::gpu::stencil_options::always_pass_no_op())
		);
		auto pipeline_state_no_depth_blend = lotus::renderer::graphics_pipeline_state(
			{ lotus::gpu::render_target_blend_options::create_default_alpha_blend(), },
			lotus::gpu::rasterizer_options(lotus::gpu::depth_bias_options::disabled(), lotus::gpu::front_facing_mode::counter_clockwise, lotus::gpu::cull_mode::cull_back, false),
			lotus::gpu::depth_stencil_options::all_disabled()
		);
		shader_types::point_shader_constants point_constants;
		const f32 tan_fov_y = std::tan(0.5f * ctx->camera_params.fov_y_radians);
		point_constants.projection_view = ctx->camera.projection_view_matrix.into<f32>();
		point_constants.view = ctx->camera.view_matrix.into<f32>();
		point_constants.down = -ctx->camera.unit_up * tan_fov_y / static_cast<f32>(size[1]);
		point_constants.right = ctx->camera.unit_right * tan_fov_y * ctx->camera_params.aspect_ratio / static_cast<f32>(size[0]);
		point_constants.opacity_multiplier = ctx->point_opacity;
		shader_types::line_shader_constants line_constants;
		line_constants.projection_view = ctx->camera.projection_view_matrix.into<f32>();
		line_constants.opacity_multiplier = ctx->line_opacity;

		auto pass = q.begin_pass(
			{ lotus::renderer::image2d_color(swap_chain, lotus::gpu::color_render_target_access::create_preserve_and_write()) },
			lotus::renderer::image2d_depth_stencil(depth_stencil, lotus::gpu::depth_render_target_access::create_preserve_and_write(), lotus::gpu::stencil_render_target_access::create_preserve_and_write()),
			size,
			u8"Debug Render"
		);
		if (!line_vertices.empty()) {
			pass.draw_instanced(
				{
					lotus::renderer::input_buffer_binding(0, line_vert_buf, 0, sizeof(line_vertex), lotus::gpu::input_buffer_rate::per_vertex, line_input_elements),
				},
				static_cast<u32>(line_vertices.size()),
				lotus::renderer::index_buffer_binding(line_idx_buf, 0, lotus::gpu::index_format::uint32),
				static_cast<u32>(line_indices.size()),
				lotus::gpu::primitive_topology::line_list,
				lotus::renderer::all_resource_bindings({}, {
					{ u8"constants", uploader.upload(line_constants) },
				}),
				ctx->line_shader_vs, ctx->line_shader_ps,
				ctx->line_depth_test ? pipeline_state_blend : pipeline_state_no_depth_blend,
				1,
				u8"Debug Lines"
			);
		}
		if (!point_vertices.empty()) {
			pass.draw_instanced(
				{
					lotus::renderer::input_buffer_binding(0, point_buf, 0, sizeof(point_vertex), lotus::gpu::input_buffer_rate::per_instance, point_input_elements),
				},
				4,
				nullptr, 0,
				lotus::gpu::primitive_topology::triangle_strip,
				lotus::renderer::all_resource_bindings({}, {
					{ u8"constants", uploader.upload(point_constants) },
				}),
				ctx->point_shader_vs, ctx->point_shader_ps,
				ctx->point_depth_test ? pipeline_state_blend : pipeline_state_no_depth_blend,
				static_cast<u32>(point_vertices.size()),
				u8"Debug Points"
			);
		}
	}

	mesh_vertices.clear();
	mesh_indices.clear();

	line_vertices.clear();
	line_indices.clear();

	point_vertices.clear();
}
