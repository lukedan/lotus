#pragma once

#include <pbd/math/vector.h>
#include <pbd/engine.h>

class debug_render {
public:
	using colorf = pbd::cvec4f;
	struct surface {
		std::vector<pbd::column_vector<3, std::size_t>> triangles;
		colorf color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};


	static void set_matrix(pbd::mat44d);
	static void set_color(colorf);
	/// Draws a sphere of radius 0.5 centered at the origin. The caller should set its color via \p glColor() and its
	/// transformation using a matrix.
	static void draw_sphere();


	void draw(bool wireframe_surfaces) const;


	static void draw_body(const pbd::shapes::plane&);
	static void draw_body(const pbd::shapes::sphere&);

	std::vector<surface> surfaces;
	std::vector<colorf> object_colors;
	const pbd::engine *engine = nullptr;
};
