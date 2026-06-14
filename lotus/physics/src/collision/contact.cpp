#include "lotus/collision/contact.h"

/// \file
/// Implementation of rigid body contact handling.

#include "lotus/collision/algorithms/gjk.h"
#include "lotus/collision/algorithms/epa.h"

namespace lotus::collision::contact {
	/// Fallback case for collision detection between generic shapes - this always returns \p std::nullopt and
	/// should only be used internally.
	template <typename Shape1, typename Shape2> [[nodiscard]] std::optional<contact_manifold> detect(
		const Shape1&, const body_position&, const Shape2&, const body_position&
	) {
		return std::nullopt;
	}

	std::optional<contact_manifold> detect(
		const shape &s1, const body_position &st1, const shape &s2, const body_position &st2
	) {
		crash_if(s1.get_type() > s2.get_type());
		return std::visit([&](const auto &shape1, const auto &shape2) {
			return detect(shape1, st1, shape2, st2);
		}, s1.value, s2.value);
	}

	std::optional<contact_manifold> detect(
		const shapes::plane&, const body_position&, const shapes::sphere&, const body_position&
	) {
		std::abort();
		return std::nullopt; // TODO
	}

	std::optional<contact_manifold> detect(
		const shapes::sphere&, const body_position&,
		const shapes::sphere&, const body_position&
	) {
		std::abort();
		return std::nullopt; // TODO
	}

	std::optional<contact_manifold> detect(
		const shapes::plane&, const body_position &s1,
		const shapes::convex_polyhedron &p2, const body_position &s2
	) {
		const vec3 norm_world = s1.orientation.rotate(vec3(0.0f, 0.0f, 1.0f));
		const vec3 norm_local2 = s2.orientation.inverse().rotate(norm_world);
		const vec3 plane_pos = s2.global_to_local(s1.position);
		contact_manifold result;
		result.normal = norm_world;
		for (const vec3 &vert : p2.vertices) {
			const scalar depth = vec::dot(vert - plane_pos, norm_local2);
			if (depth < 0.0f) {
				const vec3 projected_pos = s2.local_to_global(vert) - norm_world * depth;
				result.points.emplace_back(s1.global_to_local(projected_pos), vert);
			}
		}
		if (result.points.empty()) {
			return std::nullopt;
		}
		return result;
	}

	std::optional<contact_manifold> detect(
		const shapes::sphere&, const body_position&,
		const shapes::convex_polyhedron&, const body_position&
	) {
		return std::nullopt; // TODO
	}

	std::optional<contact_manifold> detect(
		const shapes::convex_polyhedron &p1, const body_position &s1,
		const shapes::convex_polyhedron &p2, const body_position &s2
	) {
		const polyhedron_pair pair(p1, s1, p2, s2);

		const gjk::result gjk_res = gjk::gjk(pair);
		if (!gjk_res.has_intersection) {
			return std::nullopt;
		}
		const epa::result epa_res = epa::epa(pair, gjk_res);
		return contact_manifold::compute_for_polyhedra(pair, epa_res);
	}
}
