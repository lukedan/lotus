#pragma once

#include <vector>
#include <list>

#include <pbd/math/vector.h>
#include <pbd/engine.h>

struct draw_options {
	bool wireframe_surfaces = false;
	bool wireframe_bodies = false;
	bool body_velocity = true;
	bool contacts = false;
};

class debug_render {
public:
	using colorf = pbd::cvec4f;
	struct surface_visual {
		std::vector<pbd::column_vector<3, std::size_t>> triangles;
		colorf color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};
	struct body_visual {
		std::vector<pbd::column_vector<3, std::size_t>> triangles;
		colorf color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	static void set_matrix(pbd::mat44d);
	static void set_color(colorf);
	/// Sets up basic OpenGL state for drawing.
	static void setup_draw();
	/// Draws a sphere of radius 0.5 centered at the origin. The caller should set its color via \p glColor() and its
	/// transformation using a matrix.
	static void draw_sphere();
	static void draw_sphere_wireframe();


	void draw(draw_options opt) const;


	static void draw_body(const pbd::shapes::plane&, const body_visual*, bool wireframe);
	static void draw_body(const pbd::shapes::sphere&, const body_visual*, bool wireframe);
	static void draw_body(const pbd::shapes::polyhedron&, const body_visual*, bool wireframe);

	std::vector<surface_visual> surfaces;
	std::list<body_visual> body_visuals;
	const pbd::engine *engine = nullptr;
};
