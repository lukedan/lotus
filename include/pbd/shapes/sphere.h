#pragma once

/// \file
/// Spheres.

#include "../math/constants.h"
#include "../body.h"

namespace pbd::shapes {
	/// A sphere centered at the origin.
	struct sphere {
		/// Initializes \ref radius.
		constexpr explicit sphere(double r) : radius(r) {
		}

		double radius; ///< The radius of this sphere.

		/// Returns the body properties this shape with the given density.
		[[nodiscard]] constexpr body_properties get_body_properties(double density) const {
			double mass = (4.0 / 3.0) * pi * radius * radius * radius;
			double diag = 0.4 * mass * radius * radius;
			return body_properties(mass, zero, mat33d::diagonal({ diag, diag, diag }));
		}
	};
}
