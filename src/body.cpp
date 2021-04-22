#include "pbd/body.h"

/// \file
/// Implementation of body-related functions.

#include "pbd/math/vector.h"
#include "pbd/math/quaternion.h"

namespace pbd {
	body::correction body::correction::compute(
		body &b1, body &b2, cvec3d r1, cvec3d r2, cvec3d dir, double c
	) {
		correction result = uninitialized;
		result.body1 = &b1;
		result.body2 = &b2;
		result.direction = dir;
		cvec3d n1 = b1.state.rotation.inverse().rotate(dir);
		cvec3d n2 = b2.state.rotation.inverse().rotate(dir);
		cvec3d rot1 = vec::cross(r1, n1);
		cvec3d rot2 = vec::cross(r2, n2);
		result.rotation1 = b1.properties.inverse_inertia * rot1;
		result.rotation2 = b2.properties.inverse_inertia * rot2;
		double w1 = b1.properties.inverse_mass + vec::dot(rot1, result.rotation1);
		double w2 = b2.properties.inverse_mass + vec::dot(rot2, result.rotation2);
		result.delta_lambda = -c / (w1 + w2);
		return result;
	}

	void body::correction::apply_position(double &lambda) const {
		lambda += delta_lambda;
		cvec3d p = direction * delta_lambda;
		body1->state.position += p * body1->properties.inverse_mass;
		body2->state.position -= p * body2->properties.inverse_mass;
		body1->state.rotation = quat::unsafe_normalize(
			body1->state.rotation + body1->state.rotation * quatd::from_vector((0.5 * delta_lambda) * rotation1)
		);
		body2->state.rotation = quat::unsafe_normalize(
			body2->state.rotation - body2->state.rotation * quatd::from_vector((0.5 * delta_lambda) * rotation2)
		);
	}

	void body::correction::apply_velocity(double mag) const {
		double p_norm = -mag * delta_lambda;
		cvec3d p = direction * p_norm;
		body1->state.linear_velocity += p * body1->properties.inverse_mass;
		body2->state.linear_velocity -= p * body2->properties.inverse_mass;
		body1->state.angular_velocity +=
			p_norm * body1->state.rotation.rotate(rotation1);
		body2->state.angular_velocity -=
			p_norm * body2->state.rotation.rotate(rotation2);
	}
}
