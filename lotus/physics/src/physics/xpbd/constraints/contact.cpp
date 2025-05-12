#include "lotus/physics/xpbd/constraints/contact.h"

/// \file
/// Implementation of contact constraints.

#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"

namespace lotus::physics::xpbd::constraints {
	body_contact::correction body_contact::correction::compute(
		body &b1, body &b2, vec3 r1, vec3 r2, vec3 dir, scalar c
	) {
		correction result = uninitialized;
		result.body1 = &b1;
		result.body2 = &b2;
		result.direction = dir;
		const vec3 n1 = b1.state.position.orientation.inverse().rotate(dir);
		const vec3 n2 = b2.state.position.orientation.inverse().rotate(dir);
		const vec3 rot1 = vec::cross(r1, n1);
		const vec3 rot2 = vec::cross(r2, n2);
		result.rotation1 = b1.properties.inverse_inertia * rot1;
		result.rotation2 = b2.properties.inverse_inertia * rot2;
		const scalar w1 = b1.properties.inverse_mass + vec::dot(rot1, result.rotation1);
		const scalar w2 = b2.properties.inverse_mass + vec::dot(rot2, result.rotation2);
		result.delta_lambda = -c / (w1 + w2);
		return result;
	}

	void body_contact::correction::apply_position(scalar &lambda) const {
		lambda += delta_lambda;
		const vec3 p = direction * delta_lambda;
		body1->state.position.position += p * body1->properties.inverse_mass;
		body2->state.position.position -= p * body2->properties.inverse_mass;
		body1->state.position.orientation = quat::unsafe_normalize(
			body1->state.position.orientation +
			body1->state.position.orientation * quats::from_vector((0.5f * delta_lambda) * rotation1)
		);
		body2->state.position.orientation = quat::unsafe_normalize(
			body2->state.position.orientation -
			body2->state.position.orientation * quats::from_vector((0.5f * delta_lambda) * rotation2)
		);
	}

	void body_contact::correction::apply_velocity(scalar mag) const {
		const scalar p_norm = -mag * delta_lambda;
		const vec3 p = direction * p_norm;
		body1->state.velocity.linear += p * body1->properties.inverse_mass;
		body2->state.velocity.linear -= p * body2->properties.inverse_mass;
		body1->state.velocity.angular += p_norm * body1->state.position.orientation.rotate(rotation1);
		body2->state.velocity.angular -= p_norm * body2->state.position.orientation.rotate(rotation2);
	}
}
