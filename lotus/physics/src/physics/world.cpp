#include "lotus/physics/world.h"

/// \file
/// Implementation of the physics world.

#include "lotus/collision/algorithms/gjk.h"
#include "lotus/collision/algorithms/epa.h"

namespace lotus::physics {
	std::vector<world::collision_info> world::detect_collisions() const {
		std::vector<collision_info> result;
		for (size_t i = 0; i < _bodies.size(); ++i) {
			for (size_t j = i + 1; j < _bodies.size(); ++j) {
				body *body1 = _bodies[i];
				body *body2 = _bodies[j];
				if (body1->body_shape->get_type() > body2->body_shape->get_type()) {
					std::swap(body1, body2);
				}
				const std::optional<contact_info> col = detect_collision(
					*body1->body_shape, body1->state.position, *body2->body_shape, body2->state.position
				);
				if (col) {
					result.emplace_back(*body1, *body2, col.value());
				}
			}
		}
		return result;
	}

	template <
		typename Shape1, typename Shape2
	> [[nodiscard]] std::optional<world::contact_info> world::detect_collision(
		const Shape1&, const body_position&, const Shape2&, const body_position&
	) {
		return std::nullopt;
	}

	std::optional<world::contact_info> world::detect_collision(
		const collision::shape &s1, const body_position &st1, const collision::shape &s2, const body_position &st2
	) {
		crash_if(s1.get_type() > s2.get_type());
		return std::visit([&](const auto &shapea, const auto &shapeb) {
			return detect_collision(shapea, st1, shapeb, st2);
		}, s1.value, s2.value);
	}

	std::optional<world::contact_info> world::detect_collision(
		const collision::shapes::plane&, const body_position&, const collision::shapes::sphere&, const body_position&
	) {
		return std::nullopt; // TODO
	}

	std::optional<world::contact_info> world::detect_collision(
		const collision::shapes::sphere&, const body_position&,
		const collision::shapes::sphere&, const body_position&
	) {
		return std::nullopt; // TODO
	}

	std::optional<world::contact_info> world::detect_collision(
		const collision::shapes::plane&, const body_position &s1,
		const collision::shapes::convex_polyhedron &p2, const body_position &s2
	) {
		const vec3 norm_world = s1.orientation.rotate(vec3(0.0f, 0.0f, 1.0f));
		const vec3 norm_local2 = s2.orientation.inverse().rotate(norm_world);
		const vec3 plane_pos = s2.global_to_local(s1.position);
		scalar min_depth = 0.0f;
		std::optional<vec3> contact;
		for (const auto &vert : p2.vertices) {
			scalar depth = vec::dot(vert - plane_pos, norm_local2);
			if (depth < min_depth) {
				min_depth = depth;
				contact = vert;
			}
		}
		if (contact) {
			vec3 vert_world = s2.local_to_global(contact.value());
			vert_world -= norm_world * min_depth;
			return contact_info(s1.global_to_local(vert_world), contact.value(), norm_world);
		}
		return std::nullopt;
	}

	std::optional<world::contact_info> world::detect_collision(
		const collision::shapes::sphere&, const body_position&,
		const collision::shapes::convex_polyhedron&, const body_position&
	) {
		return std::nullopt; // TODO
	}

	std::optional<world::contact_info> world::detect_collision(
		const collision::shapes::convex_polyhedron &p1, const body_position &s1,
		const collision::shapes::convex_polyhedron &p2, const body_position &s2
	) {
		const collision::polyhedron_pair pair(p1, s1, p2, s2);

		const collision::gjk::result gjk_res = collision::gjk::gjk(pair);
		if (!gjk_res.has_intersection) {
			return std::nullopt;
		}
		const collision::epa::result epa_res = collision::epa::epa(pair, gjk_res);

		contact_info result = uninitialized;
		result.normal = epa_res.normal;
		bool face_p1 =
			epa_res.vertices[0].index2 == epa_res.vertices[1].index2 &&
			epa_res.vertices[0].index2 == epa_res.vertices[2].index2;
		bool face_p2 =
			epa_res.vertices[0].index1 == epa_res.vertices[1].index1 &&
			epa_res.vertices[0].index1 == epa_res.vertices[2].index1;
		if (face_p1) { // a vertex from p2 and a face from p1
			result.contact2 = p2.vertices[epa_res.vertices[0].index2];
			const vec3 contact1 = s2.local_to_global(result.contact2) + epa_res.penetration_depth * epa_res.normal;
			result.contact1 = s1.global_to_local(contact1);
		} else if (face_p2) { // a vertex from p1 and a face from p2
			result.contact1 = p1.vertices[epa_res.vertices[0].index1];
			const vec3 contact2 = s1.local_to_global(result.contact1) - epa_res.penetration_depth * epa_res.normal;
			result.contact2 = s2.global_to_local(contact2);
		} else { // two edges
			std::array<vec3, 3> spx_pos = epa_res.simplex_positions;
			std::array<collision::simplex_vertex, 3> spx_id = epa_res.vertices;

			// adjust the arrays so that the two edges can be easily determined
			if (spx_id[0].index1 != spx_id[1].index1 && spx_id[0].index2 != spx_id[1].index2) {
				// spx_id[2] is the common vertex
				std::swap(spx_pos[0], spx_pos[2]);
				std::swap(spx_id[0], spx_id[2]);
			} else if (spx_id[0].index1 != spx_id[2].index1 && spx_id[0].index2 != spx_id[2].index2) {
				// spx_id[1] is the common vertex
				std::swap(spx_pos[0], spx_pos[1]);
				std::swap(spx_id[0], spx_id[1]);
			}
			if (spx_id[0].index1 == spx_id[1].index1) {
				std::swap(spx_pos[1], spx_pos[2]);
				std::swap(spx_id[1], spx_id[2]);
			}

			// solve for barycentric coordinates
			vec3 diff12 = spx_pos[1] - spx_pos[0]; // also the x axis
			vec3 diff13 = spx_pos[2] - spx_pos[0];
			vec3 y = vec::cross(epa_res.normal, diff12);
			auto xform = mat::concat_columns(diff12 / diff12.squared_norm(), y / y.squared_norm()).transposed();
			cvec2<scalar> pos1 = xform * diff13;
			vec3 contact_offset = epa_res.penetration_depth * epa_res.normal - spx_pos[0];
			cvec2<scalar> contact = xform * contact_offset;
			// [cx] = [1 px][bx]
			// [cy]   [0 py][by]
			//
			// [bx] = 1 / py [py -px][cx] = [cx - px * cy / py]
			// [by]          [0   1 ][cy]   [     cy / py     ]
			scalar y_ratio = contact[1] / pos1[1];
			cvec2<scalar> barycentric(contact[0] - pos1[0] * y_ratio, y_ratio);

			vec3 local_contact2 =
				p2.vertices[spx_id[0].index2] * (1.0f - barycentric[1]) +
				p2.vertices[spx_id[2].index2] * barycentric[1];
			vec3 local_contact1 =
				p1.vertices[spx_id[0].index1] * (1.0f - barycentric[0]) +
				p1.vertices[spx_id[1].index1] * barycentric[0];
			result.contact1 = local_contact1;
			result.contact2 = local_contact2;
		}
		return result;
	}
}
