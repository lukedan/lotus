#pragma once

/// \file
/// Simple shapes.

#include "pbd/math/constants.h"
#include "pbd/math/vector.h"
#include "pbd/body_properties.h"

namespace pbd::shapes {
	/// A sphere centered at the origin.
	struct sphere {
		/// No initialization.
		sphere(uninitialized_t) {
		}

		/// Creates a new uniform sphere shape with the given radius.
		[[nodiscard]] inline static sphere from_radius(double r) {
			sphere result = uninitialized;
			result.offset = zero;
			result.radius = r;
			return result;
		}

		/// The offset of the center of this sphere in local coordinates. This ensures that the center of mass is
		/// always at the origin of the local coordinate system.
		cvec3d offset = uninitialized;
		double radius; ///< The radius of this sphere.

		/// Returns the body properties of this shape with the given density.
		[[nodiscard]] constexpr body_properties get_body_properties(double density) const {
			// TODO offset this sphere
			double mass = (4.0 / 3.0) * pi * radius * radius * radius * density;
			double diag = 0.4 * mass * radius * radius;
			return body_properties::create(mat33d::diagonal(diag, diag, diag), mass);
		}
	};

	/// An infinitely large plane that passes through the origin and spans through the X-Y plane.
	struct plane {
	};
}
