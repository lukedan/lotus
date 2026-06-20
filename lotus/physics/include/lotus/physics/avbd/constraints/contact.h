#pragma once

/// \file
/// Contact constraints for AVBD.

#include "lotus/collision/algorithms/contact_manifold.h"
#include "lotus/physics/body.h"

namespace lotus::physics::avbd::constraints {
	/// Rigid body contact constraint.
	struct rigid_body_contact {
		/// A contact point.
		struct point {
			vec3 local_position1 = zero; ///< Local position of the contact point on the first body.
			vec3 local_position2 = zero; ///< Local position of the contact point on the second body.
		};

		body *body1 = nullptr; ///< The first body involved.
		body *body2 = nullptr; ///< The second body involved.
		/// Contact tangent frame in world space. The normal points out of the first body and into the second body.
		tangent_frame<scalar> tangents = zero;
		short_vector<point, 6> contact_points; ///< Contact points.
	};
}
