#pragma once

/// \file
/// Contact constraints for AVBD.

#include "lotus/collision/algorithms/contact_manifold.h"
#include "lotus/physics/body.h"

namespace lotus::physics::avbd::constraints {
	/// Rigid body contact constraint.
	struct rigid_body_contact {
		body *body1 = nullptr; ///< The first body involved.
		body *body2 = nullptr; ///< The second body involved.
		/// Contact tangent frame in world space. The normal points out of the first body and into the second body.
		tangent_frame<scalar> tangents = zero;
		short_vector<collision::contact_manifold::point, 6> contact_points; ///< Contact points.

		vec3 stiffness = zero; ///< Soft stiffness.
		vec3 lambda = zero; ///< The dual variable used to emulate a stiff constraint.
	};
}
