#include "lotus/collision/common.h"

/// \file
/// Implementation of common functions.

#include "lotus/collision/shapes/polyhedron.h"

namespace lotus::collision {
	simplex_vertex polyhedron_pair::support_vertex(vec3 dir) const {
		return simplex_vertex(
			shape1->get_support_vertex(position1.orientation.inverse().rotate(dir)).first,
			shape2->get_support_vertex(-position2.orientation.inverse().rotate(dir)).first
		);
	}

	vec3 polyhedron_pair::simplex_vertex_position(simplex_vertex v) const {
		return
			position1.local_to_global(shape1->vertices[v.index1].into<scalar>()) -
			position2.local_to_global(shape2->vertices[v.index2].into<scalar>());
	}

	polyhedron_pair::axis_projection_result polyhedron_pair::penetration_distance(vec3 axis) const {
		const shapes::polyhedron::axis_projection proj1 =
			shape1->project_onto_axis_with_transform(axis, position1);
		const shapes::polyhedron::axis_projection proj2 =
			shape2->project_onto_axis_with_transform(axis, position2);
		const scalar dist12 = proj2.min - proj1.max;
		const scalar dist21 = proj1.min - proj2.max;
		return axis_projection_result(std::max(dist12, dist21), dist12 > dist21);
	}
}
