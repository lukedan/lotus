#include "utils.h"

#include <vector>

#include <lotus/math/constants.h>

#include <lotus/renderer/shader_types_include_wrapper.h>
namespace shader_types {
#include "shaders/shader_types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>

void debug_render::draw_point(lotus::cvec3d p, lotus::linear_rgba_f color) {
	auto vert = point_vertices.emplace_back();
	vert.position = p.into<float>();
	vert.normal   = lotus::zero;
	vert.color    = lotus::cvec4f(color.into_vector().block<3, 1>(0, 0), 0.0f);
}

void debug_render::draw_line(lotus::cvec3d a, lotus::cvec3d b, lotus::linear_rgba_f color) {
	auto first_index = line_vertices.size();
	auto v1 = line_vertices.emplace_back();
	lotus::cvec4f vert_color = lotus::cvec4f(color.into_vector().block<3, 1>(0, 0), 0.0f);
	v1.position = a.into<float>();
	v1.normal   = lotus::zero;
	v1.color    = vert_color;
	auto v2 = line_vertices.emplace_back();
	v2.position = b.into<float>();
	v2.normal   = lotus::zero;
	v2.color    = vert_color;
	line_indices.emplace_back(first_index);
	line_indices.emplace_back(first_index + 1);
}

void debug_render::draw_body(
	std::span<const lotus::cvec3d> positions,
	std::span<const lotus::cvec3d> normals,
	std::span<const std::uint32_t> indices,
	lotus::mat44d transform,
	lotus::linear_rgba_f color,
	bool wireframe
) {
	std::vector<lotus::cvec3d> generated_normals;
	if (normals.empty()) {
		generated_normals.resize(positions.size(), lotus::zero);
		for (std::size_t i = 0; i < indices.size(); i += 3) {
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
	auto first_vert = verts.size();
	auto full_color = lotus::cvec4f(color.into_vector().block<3, 1>(0, 0), wireframe ? 0.0f : 1.0f);
	auto normal_transform = transform.block<3, 3>(0, 0).inverse().transposed();
	for (std::size_t i = 0; i < positions.size(); ++i) {
		auto &vert = verts.emplace_back();
		vert.position = (transform * lotus::cvec4d(positions[i], 1.0f)).block<3, 1>(0, 0).into<float>();
		vert.color    = full_color;
		vert.normal   = lotus::vec::unsafe_normalize(normal_transform * normals[i]).into<float>();
	}
	for (std::size_t i = 0; i < indices.size(); i += 3) {
		if (wireframe) {
			for (std::size_t j = 0; j < 3; ++j) {
				auto cur = indices[j];
				auto next = indices[(j + 1) % 3];
				if (cur < next) {
					line_indices.emplace_back(cur + first_vert);
					line_indices.emplace_back(next + first_vert);
				}
			}
		} else {
			for (std::size_t j = 0; j < 3; ++j) {
				mesh_indices.emplace_back(indices[i + j] + first_vert);
			}
		}
	}}

void debug_render::draw_sphere(lotus::mat44d transform, lotus::linear_rgba_f color, bool wireframe) {
	constexpr std::uint32_t _z_slices = 10;
	constexpr std::uint32_t _xy_slices = 30;
	constexpr double _z_slice_angle = lotus::pi / _z_slices;
	constexpr double _xy_slice_angle = 2.0 * lotus::pi / _xy_slices;

	static std::vector<lotus::cvec3d> _vertices;
	static std::vector<std::uint32_t> _indices;

	if (_vertices.empty()) {
		// http://www.songho.ca/opengl/gl_sphere.html
		for (std::uint32_t i = 0; i <= _z_slices; ++i) {
			double z_angle = lotus::pi / 2 - static_cast<double>(i) * _z_slice_angle;
			double xy = 0.5 * std::cos(z_angle);
			double z = 0.5 * std::sin(z_angle);

			for (std::uint32_t j = 0; j <= _xy_slices; ++j) {
				double xy_angle = static_cast<double>(j) * _xy_slice_angle;

				double x = xy * std::cos(xy_angle);
				double y = xy * std::sin(xy_angle);
				_vertices.emplace_back(x, y, z);
			}
		}

		for (std::uint32_t i = 0; i < _z_slices; ++i) {
			std::uint32_t k1 = i * (_xy_slices + 1);
			std::uint32_t k2 = k1 + _xy_slices + 1;

			for (std::uint32_t j = 0; j < _xy_slices; ++j, ++k1, ++k2) {
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

void debug_render::draw_physics_body(const lotus::collision::shapes::plane&, lotus::mat44d transform, const body_visual *visual, bool wireframe) {
	lotus::cvec3d vx = lotus::vec::unsafe_normalize((transform * lotus::cvec4d(1.0f, 0.0f, 0.0f, 0.0f)).block<3, 1>(0, 0));
	lotus::cvec3d vy = lotus::vec::unsafe_normalize((transform * lotus::cvec4d(0.0f, 1.0f, 0.0f, 0.0f)).block<3, 1>(0, 0));
	lotus::cvec3d v0 = (transform * lotus::cvec4d(0.0f, 1.0f, 0.0f, 1.0f)).block<3, 1>(0, 0);
	lotus::linear_rgba_f color = visual ? visual->color : lotus::linear_rgba_f(1.0f, 1.0f, 1.0f, 1.0f);
	if (wireframe) {
		for (int x = -100; x <= 100; ++x) {
			draw_line(v0 + vx * x + vy * -100.0f, v0 + vx * x + vy * 100.0f, color);
			draw_line(v0 + vy * x + vx * -100.0f, v0 + vy * x + vx * 100.0f, color);
		}
	} else {
		lotus::cvec3d positions[] = {
			v0 + vx *  100.0f + vy *  100.0f,
			v0 + vx * -100.0f + vy *  100.0f,
			v0 + vx *  100.0f + vy * -100.0f,
			v0 + vx * -100.0f + vy * -100.0f,
		};
		std::uint32_t indices[] = {
			0, 1, 3, 0, 3, 2
		};
		draw_body(positions, {}, indices, lotus::mat44d::identity(), color, wireframe);
	}
}

void debug_render::draw_physics_body(const lotus::collision::shapes::sphere &sphere, lotus::mat44d transform, const body_visual *visual, bool wireframe) {
	auto mat = lotus::mat44d::identity();
	mat.set_block(0, 0, lotus::mat33d::identity() * 2.0 * sphere.radius);
	mat.set_block(0, 3, sphere.offset);
	draw_sphere(transform * mat, visual ? visual->color : lotus::linear_rgba_f(1.0f, 1.0f, 1.0f, 1.0f), wireframe);
}

void debug_render::draw_physics_body(const lotus::collision::shapes::polyhedron &poly, lotus::mat44d transform, const body_visual *visual, bool wireframe) {
	if (visual) {
		draw_body(poly.vertices, {}, visual->triangles, transform, visual->color, wireframe);
	} else {
		std::vector<std::uint32_t> indices;
		for (std::size_t i = 0; i < poly.vertices.size(); ++i) {
			for (std::size_t j = i + 1; j < poly.vertices.size(); ++j) {
				for (std::size_t k = j + 1; k < poly.vertices.size(); ++k) {
					indices.emplace_back(i);
					indices.emplace_back(j);
					indices.emplace_back(k);
				}
			}
		}
		draw_body(poly.vertices, {}, indices, transform, lotus::linear_rgba_f(1.0f, 1.0f, 1.0f, 1.0f), wireframe);
	}
}

void debug_render::draw_system(lotus::physics::engine &engine) {
	for (const lotus::physics::body &b : engine.bodies) {
		const body_visual *visual = nullptr;
		if (b.user_data) {
			visual = static_cast<const body_visual*>(b.user_data);
		}

		auto mat = lotus::mat44d::identity();
		mat.set_block(0, 0, b.state.rotation.into_matrix());
		mat.set_block(0, 3, b.state.position);

		std::visit(
			[&](const auto &shape) {
				draw_physics_body(shape, mat, visual, ctx->wireframe_bodies);
			},
			b.body_shape->value
		);
	}

	// surfaces
	std::vector<lotus::cvec3d> positions;
	if (!ctx->wireframe_surfaces) {
		for (const auto &p : engine.particles) {
			positions.emplace_back(p.state.position);
		}
	}
	for (const auto &surface : surfaces) {
		if (ctx->wireframe_surfaces) {
			for (std::size_t i = 0; i < surface.triangles.size(); i += 3) {
				auto p1 = engine.particles[surface.triangles[i]].state.position;
				auto p2 = engine.particles[surface.triangles[i + 1]].state.position;
				auto p3 = engine.particles[surface.triangles[i + 2]].state.position;
				draw_line(p1, p2, surface.color);
				draw_line(p2, p3, surface.color);
				draw_line(p3, p1, surface.color);
			}
		} else {
			draw_body(positions, {}, surface.triangles, lotus::mat44d::identity(), surface.color, false);
		}
	}

	// debug stuff
	if (ctx->draw_body_velocities) {
		for (const lotus::physics::body &b : engine.bodies) {
			draw_line(b.state.position, b.state.position + b.state.linear_velocity, lotus::linear_rgba_f(1.0f, 0.0f, 0.0f, 1.0f));
			draw_line(b.state.position, b.state.position + b.state.angular_velocity, lotus::linear_rgba_f(0.0f, 1.0f, 0.0f, 1.0f));
		}
	}

	if (ctx->draw_contacts) {
		for (const auto &c : engine.contact_constraints) {
			auto p1 = c.body1->state.position + c.body1->state.rotation.rotate(c.offset1);
			auto p2 = c.body2->state.position + c.body2->state.rotation.rotate(c.offset2);
			draw_point(p1, lotus::linear_rgba_f(0.0f, 0.0f, 1.0f, 1.0f));
			draw_point(p2, lotus::linear_rgba_f(0.0f, 0.0f, 1.0f, 1.0f));
		}
	}
}

void debug_render::flush(
	lotus::renderer::context &rctx, lotus::renderer::context::queue &q,
	lotus::renderer::image2d_color frame_buffer, lotus::renderer::image2d_depth_stencil depth_buffer, lotus::cvec2u32 size
) {
	auto upload_data_buffer = [&](lotus::gpu::buffer_usage_mask usages, std::span<const std::byte> data, std::u8string_view name) -> lotus::renderer::buffer {
		if (data.empty()) {
			return nullptr;
		}
		auto result = rctx.request_buffer(name, data.size_bytes(), usages | lotus::gpu::buffer_usage_mask::copy_destination, ctx->resource_pool);
		q.upload_buffer(result, data, 0, u8"Upload Data");
		return result;
	};

	auto mesh_vert_buf = upload_data_buffer(lotus::gpu::buffer_usage_mask::vertex_buffer, { reinterpret_cast<const std::byte*>(mesh_vertices.data()), sizeof(vertex) * mesh_vertices.size() }, u8"Mesh Vertices");
	auto mesh_idx_buf = upload_data_buffer(lotus::gpu::buffer_usage_mask::index_buffer, { reinterpret_cast<const std::byte*>(mesh_indices.data()), sizeof(uint32_t) * mesh_indices.size() }, u8"Mesh Indices");

	auto line_vert_buf = upload_data_buffer(lotus::gpu::buffer_usage_mask::vertex_buffer, { reinterpret_cast<const std::byte*>(line_vertices.data()), sizeof(vertex) * line_vertices.size() }, u8"Line Vertices");
	auto line_idx_buf = upload_data_buffer(lotus::gpu::buffer_usage_mask::index_buffer, { reinterpret_cast<const std::byte*>(line_indices.data()), sizeof(uint32_t) * line_indices.size() }, u8"Line Indices");

	auto point_buf = upload_data_buffer(lotus::gpu::buffer_usage_mask::vertex_buffer, { reinterpret_cast<const std::byte*>(point_vertices.data()), sizeof(vertex) * point_vertices.size() }, u8"Point Vertices");

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
	constants.light_direction = lotus::vec::unsafe_normalize(lotus::cvec3f(0.3f, -0.4f, -0.5f));
	constants.projection_view = ctx->camera.projection_view_matrix.into<float>();

	auto pass = q.begin_pass({ frame_buffer }, depth_buffer, size, u8"Debug Render");
	if (!mesh_vertices.empty()) {
		pass.draw_instanced(
			{
				lotus::renderer::input_buffer_binding(0, mesh_vert_buf, 0, sizeof(vertex), lotus::gpu::input_buffer_rate::per_vertex, vertex_input_elements),
			}, mesh_vertices.size(),
			lotus::renderer::index_buffer_binding(mesh_idx_buf, 0, lotus::gpu::index_format::uint32), mesh_indices.size(),
			lotus::gpu::primitive_topology::triangle_list,
			lotus::renderer::all_resource_bindings({}, {
				{ u8"constants", lotus::renderer::descriptor_resource::immediate_constant_buffer::create_for(constants) },
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
			}, line_vertices.size(),
			lotus::renderer::index_buffer_binding(line_idx_buf, 0, lotus::gpu::index_format::uint32), line_indices.size(),
			lotus::gpu::primitive_topology::line_list,
			lotus::renderer::all_resource_bindings({}, {
				{ u8"constants", lotus::renderer::descriptor_resource::immediate_constant_buffer::create_for(constants) },
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
			}, point_vertices.size(),
			nullptr, 0,
			lotus::gpu::primitive_topology::point_list,
			lotus::renderer::all_resource_bindings({}, {
				{ u8"constants", lotus::renderer::descriptor_resource::immediate_constant_buffer::create_for(constants) },
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
