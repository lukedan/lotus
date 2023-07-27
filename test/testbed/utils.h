#pragma once

#include <vector>
#include <list>

#include <lotus/color.h>
#include <lotus/math/vector.h>
#include <lotus/utils/camera.h>
#include <lotus/physics/engine.h>
#include <lotus/renderer/context/asset_manager.h>

struct test_context {
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> default_shader_vs = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> default_shader_ps = nullptr;
	lotus::camera_parameters<double> camera_params = lotus::uninitialized;
	lotus::camera<double> camera = lotus::uninitialized;
	lotus::renderer::pool resource_pool = nullptr;
	lotus::renderer::pool upload_pool = nullptr;

	bool wireframe_surfaces = false;
	bool wireframe_bodies = false;
	bool draw_body_velocities = true;
	bool draw_contacts = false;

	void update_camera() {
		camera = camera_params.into_camera();
	}
};

class debug_render {
public:
	struct surface_visual {
		std::vector<std::uint32_t> triangles;
		lotus::linear_rgba_f color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};
	struct body_visual {
		std::vector<std::uint32_t> triangles;
		lotus::linear_rgba_f color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	struct vertex {
		lotus::cvec3f position = lotus::uninitialized;
		lotus::cvec4f color = lotus::uninitialized;
		lotus::cvec3f normal = lotus::uninitialized;
	};

	void draw_point(lotus::cvec3d p, lotus::linear_rgba_f color);
	void draw_line(lotus::cvec3d a, lotus::cvec3d b, lotus::linear_rgba_f color);

	void draw_body(
		std::span<const lotus::cvec3d> positions,
		std::span<const lotus::cvec3d> normals,
		std::span<const std::uint32_t> indices,
		lotus::mat44d transform,
		lotus::linear_rgba_f color,
		bool wireframe
	);
	void draw_sphere(lotus::mat44d transform, lotus::linear_rgba_f color, bool wireframe);

	void draw_physics_body(const lotus::collision::shapes::plane&, lotus::mat44d transform, const body_visual*, bool wireframe);
	void draw_physics_body(const lotus::collision::shapes::sphere&, lotus::mat44d transform, const body_visual*, bool wireframe);
	void draw_physics_body(const lotus::collision::shapes::polyhedron&, lotus::mat44d transform, const body_visual*, bool wireframe);
	void draw_system(lotus::physics::engine&);


	void flush(
		lotus::renderer::context&, lotus::renderer::context::queue&,
		lotus::renderer::image2d_color, lotus::renderer::image2d_depth_stencil, lotus::cvec2u32 size
	);


	const test_context *ctx;

	std::vector<vertex> mesh_vertices;
	std::vector<std::uint32_t> mesh_indices;

	std::vector<vertex> line_vertices;
	std::vector<std::uint32_t> line_indices;

	std::vector<vertex> point_vertices;

	std::deque<surface_visual> surfaces;
	std::deque<body_visual> bodies;
};
