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

void debug_render::draw_point(vec3 p, lotus::linear_rgba_f color, scalar size) {
	draw_box(
		{
			{ size, 0.0f, 0.0f, p[0] },
			{ 0.0f, size, 0.0f, p[1] },
			{ 0.0f, 0.0f, size, p[2] },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
		},
		color,
		false
	);
}

void debug_render::draw_line(vec3 a, vec3 b, lotus::linear_rgba_f color) {
	color.a = 0.0f; // disable lighting

	auto first_index = static_cast<u32>(line_vertices.size());
	auto &v1 = line_vertices.emplace_back();
	v1.position = a.into<float>();
	v1.normal   = vec3(1.0f, 0.0f, 0.0f);
	v1.color    = color;
	auto &v2 = line_vertices.emplace_back();
	v2.position = b.into<float>();
	v2.normal   = vec3(1.0f, 0.0f, 0.0f);
	v2.color    = color;
	line_indices.emplace_back(first_index);
	line_indices.emplace_back(first_index + 1);
}

void debug_render::draw_body(
	std::span<const vec3> positions,
	std::span<const vec3> normals,
	std::span<const u32> indices,
	mat44s transform,
	lotus::linear_rgba_f color,
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

	auto &verts = wireframe ? line_vertices : mesh_vertices;
	auto first_vert = static_cast<u32>(verts.size());
	auto normal_transform = transform.block<3, 3>(0, 0).inverse().transposed();
	for (usize i = 0; i < positions.size(); ++i) {
		auto &vert = verts.emplace_back();
		vert.position = (transform * vec4(positions[i], 1.0f)).block<3, 1>(0, 0);
		vert.color    = color;
		vert.normal   = lotus::vecu::normalize(normal_transform * normals[i]);
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

void debug_render::draw_sphere(mat44s transform, lotus::linear_rgba_f color, bool wireframe) {
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

void debug_render::draw_box(mat44s transform, lotus::linear_rgba_f color, bool wireframe) {
	static constexpr std::array _vertices = {
		// XY negative
		vec3(-0.5f, -0.5f, -0.5f),
		vec3( 0.5f, -0.5f, -0.5f),
		vec3(-0.5f,  0.5f, -0.5f),
		vec3( 0.5f,  0.5f, -0.5f),
		// XY positive
		vec3(-0.5f, -0.5f,  0.5f),
		vec3( 0.5f, -0.5f,  0.5f),
		vec3(-0.5f,  0.5f,  0.5f),
		vec3( 0.5f,  0.5f,  0.5f),
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
		// YZ negative
		vec3(-0.5f, -0.5f, -0.5f),
		vec3(-0.5f,  0.5f, -0.5f),
		vec3(-0.5f, -0.5f,  0.5f),
		vec3(-0.5f,  0.5f,  0.5f),
		// YZ positive
		vec3( 0.5f, -0.5f, -0.5f),
		vec3( 0.5f,  0.5f, -0.5f),
		vec3( 0.5f, -0.5f,  0.5f),
		vec3( 0.5f,  0.5f,  0.5f),
	};
	static constexpr std::array _normals = {
		vec3( 0.0f,  0.0f, -1.0f), vec3( 0.0f,  0.0f, -1.0f), vec3( 0.0f,  0.0f, -1.0f), vec3( 0.0f,  0.0f, -1.0f),
		vec3( 0.0f,  0.0f,  1.0f), vec3( 0.0f,  0.0f,  1.0f), vec3( 0.0f,  0.0f,  1.0f), vec3( 0.0f,  0.0f,  1.0f),
		vec3( 0.0f, -1.0f,  0.0f), vec3( 0.0f, -1.0f,  0.0f), vec3( 0.0f, -1.0f,  0.0f), vec3( 0.0f, -1.0f,  0.0f),
		vec3( 0.0f,  1.0f,  0.0f), vec3( 0.0f,  1.0f,  0.0f), vec3( 0.0f,  1.0f,  0.0f), vec3( 0.0f,  1.0f,  0.0f),
		vec3(-1.0f,  0.0f,  0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(-1.0f,  0.0f,  0.0f),
		vec3( 1.0f,  0.0f,  0.0f), vec3( 1.0f,  0.0f,  0.0f), vec3( 1.0f,  0.0f,  0.0f), vec3( 1.0f,  0.0f,  0.0f),
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


void debug_render::draw_physics_body(const lotus::collision::shapes::plane&, mat44s transform, const body_visual *visual, bool wireframe) {
	vec3 vx = lotus::vecu::normalize((transform * vec4(1.0f, 0.0f, 0.0f, 0.0f)).block<3, 1>(0, 0));
	vec3 vy = lotus::vecu::normalize((transform * vec4(0.0f, 1.0f, 0.0f, 0.0f)).block<3, 1>(0, 0));
	vec3 v0 = (transform * vec4(0.0f, 1.0f, 0.0f, 1.0f)).block<3, 1>(0, 0);
	lotus::linear_rgba_f color = visual ? visual->color : lotus::linear_rgba_f(1.0f, 1.0f, 1.0f, 1.0f);
	if (wireframe) {
		for (int x = -100; x <= 100; ++x) {
			draw_line(v0 + vx * x + vy * -100.0f, v0 + vx * x + vy * 100.0f, color);
			draw_line(v0 + vy * x + vx * -100.0f, v0 + vy * x + vx * 100.0f, color);
		}
	} else {
		vec3 positions[] = {
			v0 + vx *  100.0f + vy *  100.0f,
			v0 + vx * -100.0f + vy *  100.0f,
			v0 + vx *  100.0f + vy * -100.0f,
			v0 + vx * -100.0f + vy * -100.0f,
		};
		u32 indices[] = {
			0, 1, 3, 0, 3, 2
		};
		draw_body(positions, {}, indices, mat44s::identity(), color, wireframe);
	}
}

void debug_render::draw_physics_body(const lotus::collision::shapes::sphere &sphere, mat44s transform, const body_visual *visual, bool wireframe) {
	auto mat = mat44s::identity();
	mat.set_block(0, 0, mat33s::identity() * 2.0f * sphere.radius);
	mat.set_block(0, 3, sphere.offset);
	draw_sphere(transform * mat, visual ? visual->color : lotus::linear_rgba_f(1.0f, 1.0f, 1.0f, 1.0f), wireframe);
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

		draw_body(verts, {}, indices, transform, lotus::linear_rgba_f(1.0f, 1.0f, 1.0f, 1.0f), wireframe);
	}
}

void debug_render::draw_world(const lotus::physics::world &world) {
	for (const lotus::physics::body *b : world.get_bodies()) {
		const body_visual *visual = nullptr;
		if (b->user_data) {
			visual = static_cast<const body_visual*>(b->user_data);
		}

		auto mat = mat44s::identity();
		mat.set_block(0, 0, b->state.position.orientation.into_matrix());
		mat.set_block(0, 3, b->state.position.position);

		std::visit(
			[&](const auto &shape) {
				draw_physics_body(shape, mat, visual, ctx->wireframe_bodies);
			},
			b->body_shape->value
		);
	}

	// debug stuff
	if (ctx->draw_body_velocities) {
		for (const lotus::physics::body *b : world.get_bodies()) {
			draw_line(b->state.position.position, b->state.position.position + b->state.velocity.linear, lotus::linear_rgba_f(1.0f, 0.0f, 0.0f, 1.0f));
			draw_line(b->state.position.position, b->state.position.position + b->state.velocity.angular, lotus::linear_rgba_f(0.0f, 1.0f, 0.0f, 1.0f));
		}
	}
}

void debug_render::draw_system(lotus::physics::rigid_body::solver &solver) {
	draw_world(*solver.physics_world);

	// debug stuff
	if (ctx->draw_contacts) {
		using contact_set_t = lotus::physics::rigid_body::constraints::contact_set_blcp;
		for (contact_set_t &contact_set : solver.contact_constraints) {
			for (usize i = 0; i < contact_set.contacts_info.size(); ++i) {
				const contact_set_t::contact_info &ci = contact_set.contacts_info[i];
				const vec3 impulse = contact_set.get_impulse(i);
				draw_point(ci.contact, lotus::linear_rgba_f(1.0f, 1.0f, 0.0f, 0.0f));
				draw_line(ci.contact, ci.contact + impulse, lotus::linear_rgba_f(1.0f, 0.0f, 0.0f, 1.0f));
			}
		}
	}
}


void debug_render::draw_system(lotus::physics::xpbd::solver &solver) {
	draw_world(*solver.physics_world);

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

	// debug stuff
	if (ctx->draw_contacts) {
		for (const auto &c : solver.contact_constraints) {
			auto p1 = c.body1->state.position.local_to_global(c.offset1);
			auto p2 = c.body2->state.position.local_to_global(c.offset2);
			draw_point(p1, lotus::linear_rgba_f(0.0f, 0.0f, 1.0f, 1.0f));
			draw_point(p2, lotus::linear_rgba_f(0.0f, 0.0f, 1.0f, 1.0f));
		}
	}
}

void debug_render::flush(
	lotus::renderer::context &rctx, lotus::renderer::context::queue &q, lotus::renderer::constant_uploader &uploader,
	lotus::renderer::image2d_color frame_buffer, lotus::renderer::image2d_depth_stencil depth_buffer, lotus::cvec2u32 size
) {
	auto upload_data_buffer = [&](lotus::renderer::context::queue &q, lotus::gpu::buffer_usage_mask usages, std::span<const std::byte> data, std::u8string_view name) -> lotus::renderer::buffer {
		if (data.empty()) {
			return nullptr;
		}
		auto result = rctx.request_buffer(name, data.size_bytes(), usages | lotus::gpu::buffer_usage_mask::copy_destination, ctx->resource_pool);
		auto temp = rctx.request_buffer(name, data.size_bytes(), lotus::gpu::buffer_usage_mask::copy_source, ctx->upload_pool);
		rctx.write_data_to_buffer(temp, data);
		q.copy_buffer(temp, result, 0, 0, data.size(), u8"Upload data buffer");
		return result;
	};

	auto mesh_vert_buf = upload_data_buffer(q, lotus::gpu::buffer_usage_mask::vertex_buffer, { reinterpret_cast<const std::byte*>(mesh_vertices.data()), sizeof(vertex) * mesh_vertices.size() }, u8"Mesh Vertices");
	auto mesh_idx_buf = upload_data_buffer(q, lotus::gpu::buffer_usage_mask::index_buffer, { reinterpret_cast<const std::byte*>(mesh_indices.data()), sizeof(u32) * mesh_indices.size() }, u8"Mesh Indices");

	auto line_vert_buf = upload_data_buffer(q, lotus::gpu::buffer_usage_mask::vertex_buffer, { reinterpret_cast<const std::byte*>(line_vertices.data()), sizeof(vertex) * line_vertices.size() }, u8"Line Vertices");
	auto line_idx_buf = upload_data_buffer(q, lotus::gpu::buffer_usage_mask::index_buffer, { reinterpret_cast<const std::byte*>(line_indices.data()), sizeof(u32) * line_indices.size() }, u8"Line Indices");

	auto point_buf = upload_data_buffer(q, lotus::gpu::buffer_usage_mask::vertex_buffer, { reinterpret_cast<const std::byte*>(point_vertices.data()), sizeof(vertex) * point_vertices.size() }, u8"Point Vertices");

	auto pipeline_state = lotus::renderer::graphics_pipeline_state(
		{ lotus::gpu::render_target_blend_options::disabled(), },
		lotus::gpu::rasterizer_options(lotus::gpu::depth_bias_options::disabled(), lotus::gpu::front_facing_mode::clockwise, lotus::gpu::cull_mode::none, false),
		lotus::gpu::depth_stencil_options(true, true, lotus::gpu::comparison_function::greater, false, 0, 0, lotus::gpu::stencil_options::always_pass_no_op(), lotus::gpu::stencil_options::always_pass_no_op())
	);
	auto vertex_input_elements = {
		lotus::gpu::input_buffer_element(u8"POSITION", 0, lotus::gpu::format::r32g32b32_float, offsetof(debug_render::vertex, position)),
		lotus::gpu::input_buffer_element(u8"COLOR", 0, lotus::gpu::format::r32g32b32a32_float, offsetof(debug_render::vertex, color)),
		lotus::gpu::input_buffer_element(u8"NORMAL", 0, lotus::gpu::format::r32g32b32_float, offsetof(debug_render::vertex, normal)),
	};

	shader_types::default_shader_constants constants;
	constants.light_direction = lotus::vecu::normalize(lotus::cvec3f(0.3f, 0.4f, 0.5f));
	constants.projection_view = ctx->camera.projection_view_matrix.into<float>();

	auto pass = q.begin_pass({ frame_buffer }, depth_buffer, size, u8"Debug Render");
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
			ctx->default_shader_vs, ctx->default_shader_ps,
			pipeline_state,
			1,
			u8"Debug Meshes"
		);
	}
	if (!line_vertices.empty()) {
		pass.draw_instanced(
			{
				lotus::renderer::input_buffer_binding(0, line_vert_buf, 0, sizeof(vertex), lotus::gpu::input_buffer_rate::per_vertex, vertex_input_elements),
			},
			static_cast<u32>(line_vertices.size()),
			lotus::renderer::index_buffer_binding(line_idx_buf, 0, lotus::gpu::index_format::uint32),
			static_cast<u32>(line_indices.size()),
			lotus::gpu::primitive_topology::line_list,
			lotus::renderer::all_resource_bindings({}, {
				{ u8"constants", uploader.upload(constants) },
			}),
			ctx->default_shader_vs, ctx->default_shader_ps,
			pipeline_state,
			1,
			u8"Debug Lines"
		);
	}
	if (!point_vertices.empty()) {
		pass.draw_instanced(
			{
				lotus::renderer::input_buffer_binding(0, point_buf, 0, sizeof(vertex), lotus::gpu::input_buffer_rate::per_vertex, vertex_input_elements),
			},
			static_cast<u32>(point_vertices.size()),
			nullptr, 0,
			lotus::gpu::primitive_topology::point_list,
			lotus::renderer::all_resource_bindings({}, {
				{ u8"constants", uploader.upload(constants) },
			}),
			ctx->default_shader_vs, ctx->default_shader_ps,
			pipeline_state,
			1,
			u8"Debug Points"
		);
	}
	pass.end();

	mesh_vertices.clear();
	mesh_indices.clear();

	line_vertices.clear();
	line_indices.clear();

	point_vertices.clear();
}
