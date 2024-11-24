#pragma once

/// \file
/// Simple shapes.

#include "lotus/math/constants.h"
#include "lotus/math/vector.h"
#include "lotus/physics/body_properties.h"

#include "lotus/collision/common.h"

namespace lotus::collision::shapes {
	/// A sphere centered at the origin.
	struct sphere {
		/// No initialization.
		sphere(uninitialized_t) {
		}
		/// Creates a new uniform sphere shape with the given radius.
		[[nodiscard]] inline static sphere from_radius(scalar r) {
			sphere result = uninitialized;
			result.offset = zero;
			result.radius = r;
			return result;
		}

		/// The offset of the center of this sphere in local coordinates. This ensures that the center of mass is
		/// always at the origin of the local coordinate system.
		vec3 offset = uninitialized;
		scalar radius; ///< The radius of this sphere.

		/// Returns the body properties of this shape with the given density.
		[[nodiscard]] constexpr physics::body_properties get_body_properties(scalar density) const {
			// TODO offset this sphere
			const scalar mass = (4.0f / 3.0f) * static_cast<scalar>(pi) * radius * radius * radius * density;
			const scalar diag = 0.4f * mass * radius * radius;
			return physics::body_properties::create(mat33s::diagonal(diag, diag, diag), mass);
		}
	};

	/// An infinitely large plane that passes through the origin and spans through the X-Y plane.
	struct plane {
	};
}
