#pragma once

#include <vector>
#include <list>

#include <lotus/math/vector.h>
#include <lotus/physics/engine.h>

struct draw_options {
	bool wireframe_surfaces = false;
	bool wireframe_bodies = false;
	bool body_velocity = true;
	bool contacts = false;
};

class debug_render {
public:
	using colorf = lotus::cvec4f;
	struct surface_visual {
		std::vector<std::array<std::size_t, 3>> triangles;
		colorf color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};
	struct body_visual {
		std::vector<std::array<std::size_t, 3>> triangles;
		colorf color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	static void set_matrix(lotus::mat44d);
	static void set_color(colorf);
	/// Sets up basic OpenGL state for drawing.
	static void setup_draw();
	/// Draws a sphere of radius 0.5 centered at the origin. The caller should set its color via \p glColor() and its
	/// transformation using a matrix.
	static void draw_sphere();
	static void draw_sphere_wireframe();


	void draw(draw_options opt) const;


	static void draw_body(const lotus::collision::shapes::plane&, const body_visual*, bool wireframe);
	static void draw_body(const lotus::collision::shapes::sphere&, const body_visual*, bool wireframe);
	static void draw_body(const lotus::collision::shapes::polyhedron&, const body_visual*, bool wireframe);

	std::vector<surface_visual> surfaces;
	std::list<body_visual> body_visuals;
	const lotus::physics::engine *engine = nullptr;
};
