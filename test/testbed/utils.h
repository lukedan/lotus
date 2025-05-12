#pragma once

#include <vector>
#include <list>

#include <lotus/color.h>
#include <lotus/math/vector.h>
#include <lotus/utils/camera.h>
#include <lotus/physics/xpbd/solver.h>
#include <lotus/renderer/context/asset_manager.h>

using namespace lotus::types;
using namespace lotus::collision::types;

using vec2 = lotus::cvec2<scalar>;
using vec3 = lotus::cvec3<scalar>;
using vec4 = lotus::cvec4<scalar>;
using mat44s = lotus::mat44<scalar>;

struct test_context {
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> default_shader_vs = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> default_shader_ps = nullptr;
	lotus::camera_parameters<scalar> camera_params = lotus::uninitialized;
	lotus::camera<scalar> camera = lotus::uninitialized;
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
		std::vector<u32> triangles;
		lotus::linear_rgba_f color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};
	struct body_visual {
		std::vector<u32> triangles;
		lotus::linear_rgba_f color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	struct vertex {
		vec3 position = lotus::uninitialized;
		lotus::linear_rgba_f color = lotus::uninitialized;
		vec3 normal = lotus::uninitialized;
	};

	void draw_point(vec3 p, lotus::linear_rgba_f color);
	void draw_line(vec3 a, vec3 b, lotus::linear_rgba_f color);

	void draw_body(
		std::span<const vec3> positions,
		std::span<const vec3> normals,
		std::span<const u32> indices,
		mat44s transform,
		lotus::linear_rgba_f color,
		bool wireframe
	);
	void draw_sphere(mat44s transform, lotus::linear_rgba_f color, bool wireframe);

	void draw_physics_body(const lotus::collision::shapes::plane&, mat44s transform, const body_visual*, bool wireframe);
	void draw_physics_body(const lotus::collision::shapes::sphere&, mat44s transform, const body_visual*, bool wireframe);
	void draw_physics_body(const lotus::collision::shapes::convex_polyhedron&, mat44s transform, const body_visual*, bool wireframe);
	void draw_system(lotus::physics::xpbd::solver&);


	void flush(
		lotus::renderer::context&, lotus::renderer::context::queue&, lotus::renderer::constant_uploader&,
		lotus::renderer::image2d_color, lotus::renderer::image2d_depth_stencil, lotus::cvec2u32 size
	);


	const test_context *ctx;

	std::vector<vertex> mesh_vertices;
	std::vector<u32> mesh_indices;

	std::vector<vertex> line_vertices;
	std::vector<u32> line_indices;

	std::vector<vertex> point_vertices;

	std::deque<surface_visual> surfaces;
	std::deque<body_visual> bodies;
};
