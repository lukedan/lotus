#pragma once

/// \file
/// Different contact types.

#include "common.h"
#include "shape.h"

namespace lotus::collision {
	/// Information about contact between two rigid bodies.
	struct rigid_body_contact {
		/// No initialization.
		rigid_body_contact(uninitialized_t) {
		}
		/// Initializes all fields of this struct.
		[[nodiscard]] rigid_body_contact(vec3 p1, vec3 p2, vec3 n) : local_pos1(p1), local_pos2(p2), normal(n) {
		}

		vec3 local_pos1 = uninitialized; ///< Contact point on the first object in local space.
		vec3 local_pos2 = uninitialized; ///< Contact point on the second object in local space.
		/// Normalized contact normal. This points out of the first body and into the second body.
		vec3 normal = uninitialized;


		/// Detects collision between two generic shapes. The type index of the first shape must be less than that of
		/// the second shape.
		[[nodiscard]] static std::optional<rigid_body_contact> detect(
			const shape&, const body_position&, const shape&, const body_position&
		);
		/// Fallback case for collision detection between generic shapes - this always returns \p std::nullopt and
		/// should only be used internally.
		template <
			typename Shape1, typename Shape2
		> [[nodiscard]] static std::optional<rigid_body_contact> detect(
			const Shape1&, const body_position&, const Shape2&, const body_position&
		);
		/// Detects collision between a sphere and a plane.
		[[nodiscard]] static std::optional<rigid_body_contact> detect(
			const shapes::plane&, const body_position&, const shapes::sphere&, const body_position&
		);
		/// Detects collision between two spheres.
		[[nodiscard]] static std::optional<rigid_body_contact> detect(
			const shapes::sphere&, const body_position&, const shapes::sphere&, const body_position&
		);
		/// Detects collision between a plane and a convex polyhedron.
		[[nodiscard]] static std::optional<rigid_body_contact> detect(
			const shapes::plane&, const body_position&, const shapes::convex_polyhedron&, const body_position&
		);
		/// Detects collision between a sphere and a convex polyhedron.
		[[nodiscard]] static std::optional<rigid_body_contact> detect(
			const shapes::sphere&, const body_position&, const shapes::convex_polyhedron&, const body_position&
		);
		/// Detects collision between two polyhedra.
		[[nodiscard]] static std::optional<rigid_body_contact> detect(
			const shapes::convex_polyhedron&, const body_position&,
			const shapes::convex_polyhedron&, const body_position&
		);
	};
}
