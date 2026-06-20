#pragma once

#include <vector>
#include <list>

#include <lotus/color.h>
#include <lotus/math/vector.h>
#include <lotus/utils/camera.h>
#include <lotus/physics/avbd/solver.h>
#include <lotus/physics/sequential_impulse/solver.h>
#include <lotus/physics/xpbd/solver.h>
#include <lotus/renderer/context/asset_manager.h>

using namespace lotus::types;
using namespace lotus::vector_types;
using namespace lotus::collision::types;

struct test_context {
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> gbuffer_shader_vs = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> gbuffer_shader_ps = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> point_shader_vs = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> point_shader_ps = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> line_shader_vs = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> line_shader_ps = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> shadow_vs = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> fullscreen_quad_vs = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> light_quad_ps = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> ssao_cs = nullptr;
	lotus::renderer::assets::handle<lotus::renderer::assets::shader> sky_ps = nullptr;
	lotus::renderer::assets::manager *asset_manager = nullptr;
	lotus::camera_parameters<scalar> camera_params = lotus::uninitialized;
	lotus::camera<scalar> camera = lotus::uninitialized;
	lotus::renderer::pool resource_pool = nullptr;
	lotus::renderer::pool upload_pool = nullptr;

	bool wireframe_surfaces = false;
	bool wireframe_bodies = false;
	bool draw_body_velocities = true;
	bool draw_contact_points = true;
	bool draw_contact_normals = false;
	bool draw_contact_relationships = false;
	bool draw_particles = true;
	bool draw_shadows = true;
	bool draw_orientations = true;
	bool draw_faces = false;
	bool point_depth_test = false;
	bool line_depth_test = false;
	f32 point_size = 7.0f;
	f32 point_opacity = 0.5f;
	f32 line_opacity = 1.0f;
	f32 ssao_smoothing = 0.3f;

	void update_camera() {
		camera = camera_params.into_camera();
	}
};

class debug_render {
public:
	struct surface_visual {
		std::vector<u32> triangles;
		lotus::linear_rgba_f32 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};
	struct body_visual {
		std::vector<u32> triangles;
		lotus::linear_rgba_f32 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	struct vertex {
		cvec3f32 position = lotus::zero;
		cvec3f32 position_ls = lotus::zero;
		lotus::linear_rgba_f32 color = lotus::zero;
		cvec3f32 normal = lotus::zero;
	};
	struct point_vertex {
		cvec3f32 position = lotus::zero;
		lotus::linear_rgba_f32 color = lotus::zero;
		f32 size = 0;
	};
	struct line_vertex {
		cvec3f32 position = lotus::zero;
		lotus::linear_rgba_f32 color = lotus::zero;
	};

	void draw_point(vec3 p, lotus::linear_rgba_f32 color, scalar size = 0.0f);
	void draw_line(vec3 a, vec3 b, lotus::linear_rgba_f32 color);
	void draw_line(vec3 a, vec3 b, mat44s transform, lotus::linear_rgba_f32 color) {
		const vec3 ta = (transform * vec4(a, 1.0f)).block<3, 1>(0, 0);
		const vec3 tb = (transform * vec4(b, 1.0f)).block<3, 1>(0, 0);
		draw_line(ta, tb, color);
	}

	void draw_body(
		std::span<const vec3> positions,
		std::span<const vec3> normals,
		std::span<const u32> indices,
		mat44s transform,
		lotus::linear_rgba_f32 color,
		bool wireframe
	);
	void draw_sphere(mat44s transform, lotus::linear_rgba_f32 color, bool wireframe);
	void draw_box(mat44s transform, lotus::linear_rgba_f32 color, bool wireframe);

	void draw_frame(uquats ori, vec3 pos, scalar size);

	void draw_physics_body(const lotus::collision::shapes::plane&, mat44s transform, const body_visual*, bool wireframe);
	void draw_physics_body(const lotus::collision::shapes::sphere&, mat44s transform, const body_visual*, bool wireframe);
	void draw_physics_body(const lotus::collision::shapes::convex_polyhedron&, mat44s transform, const body_visual*, bool wireframe);
	void draw_world(const lotus::physics::world&);
	void draw_system(lotus::physics::avbd::solver&);
	void draw_system(lotus::physics::sequential_impulse::solver&);
	void draw_system(lotus::physics::xpbd::solver&);


	void flush(
		lotus::renderer::context&, lotus::renderer::context::queue&, lotus::renderer::constant_uploader&,
		lotus::renderer::recorded_resources::swap_chain, cvec2u32 size
	);


	const test_context *ctx;

	std::vector<vertex> mesh_vertices;
	std::vector<u32> mesh_indices;

	std::vector<line_vertex> line_vertices;
	std::vector<u32> line_indices;

	std::vector<point_vertex> point_vertices;

	std::deque<surface_visual> surfaces;
	std::deque<body_visual> bodies;
};

/// Creates a box centered at the origin with the given size.
inline std::pair<
	lotus::collision::shapes::convex_polyhedron,
	lotus::collision::shapes::convex_polyhedron::properties
> create_box_shape(vec3 size) {
	const vec3 half_size = size * 0.5f;
	std::vector<vec3> box_verts;
	box_verts.emplace_back( half_size[0],  half_size[1],  half_size[2]);
	box_verts.emplace_back( half_size[0],  half_size[1], -half_size[2]);
	box_verts.emplace_back( half_size[0], -half_size[1],  half_size[2]);
	box_verts.emplace_back( half_size[0], -half_size[1], -half_size[2]);
	box_verts.emplace_back(-half_size[0],  half_size[1],  half_size[2]);
	box_verts.emplace_back(-half_size[0],  half_size[1], -half_size[2]);
	box_verts.emplace_back(-half_size[0], -half_size[1],  half_size[2]);
	box_verts.emplace_back(-half_size[0], -half_size[1], -half_size[2]);
	return lotus::collision::shapes::convex_polyhedron::bake(box_verts);
}
