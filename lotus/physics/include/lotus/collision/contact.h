#pragma once

/// \file
/// Different contact types.

#include "lotus/collision/common.h"
#include "lotus/collision/shape.h"
#include "lotus/collision/algorithms/contact_manifold.h"

namespace lotus::collision::contact {
	/// Detects collision between two generic shapes. The type index of the first shape must be less than that of
	/// the second shape.
	[[nodiscard]] std::optional<contact_manifold> detect(
		const shape&, const body_position&, const shape&, const body_position&
	);

	/// Fallback case for collision detection between generic shapes - this always returns \p std::nullopt and
	/// should only be used internally.
	template <
		typename Shape1, typename Shape2
	> [[nodiscard]] std::optional<contact_manifold> detect(
		const Shape1&, const body_position&, const Shape2&, const body_position&
	);

	/// Detects collision between a sphere and a plane.
	[[nodiscard]] std::optional<contact_manifold> detect(
		const shapes::plane&, const body_position&, const shapes::sphere&, const body_position&
	);
	/// Detects collision between two spheres.
	[[nodiscard]] std::optional<contact_manifold> detect(
		const shapes::sphere&, const body_position&, const shapes::sphere&, const body_position&
	);
	/// Detects collision between a plane and a convex polyhedron.
	[[nodiscard]] std::optional<contact_manifold> detect(
		const shapes::plane&, const body_position&, const shapes::convex_polyhedron&, const body_position&
	);
	/// Detects collision between a sphere and a convex polyhedron.
	[[nodiscard]] std::optional<contact_manifold> detect(
		const shapes::sphere&, const body_position&, const shapes::convex_polyhedron&, const body_position&
	);
	/// Detects collision between two polyhedra.
	[[nodiscard]] std::optional<contact_manifold> detect(
		const shapes::convex_polyhedron&, const body_position&,
		const shapes::convex_polyhedron&, const body_position&
	);
}
