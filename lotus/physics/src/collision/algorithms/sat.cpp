#include "lotus/collision/algorithms/sat.h"

/// \file
/// Implementation of the separation axis test.

namespace lotus::collision {
	sat_t::result sat_t::operator()(polyhedron_pair input) {
		polyhedron_pair::axis_projection_result max_proj = std::nullopt;
		vec3 max_proj_axis = uninitialized;

		// test all faces of the first polyhedron
		for (const vec3 axis : input.shape1->unique_face_normals) {
			const polyhedron_pair::axis_projection_result proj_res = input.penetration_distance(axis);
			if (proj_res.distance > max_proj.distance) {
				max_proj = proj_res;
				max_proj_axis = axis;
			}
		}

		// test all faces of the second polyhedron
		for (const vec3 axis : input.shape2->unique_face_normals) {
			const polyhedron_pair::axis_projection_result proj_res = input.penetration_distance(axis);
			if (proj_res.distance > max_proj.distance) {
				max_proj = proj_res;
				max_proj_axis = axis;
			}
		}

		// test all edges
		for (const vec3 edge1 : input.shape1->unique_edge_directions) {
			for (const vec3 edge2 : input.shape2->unique_edge_directions) {
				const vec3 axis = vec::cross(edge1, edge2);
				const polyhedron_pair::axis_projection_result proj_res = input.penetration_distance(axis);
				if (proj_res.distance > max_proj.distance) {
					max_proj = proj_res;
					max_proj_axis = axis;
				}
			}
		}

		return result(max_proj.shape2_after_shape2 ? max_proj_axis : -max_proj_axis, max_proj.distance);
	}
}
