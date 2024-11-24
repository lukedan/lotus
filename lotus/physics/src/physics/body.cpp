#include "lotus/physics/body.h"

/// \file
/// Implementation of body-related functions.

#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"

namespace lotus::physics {
	body::correction body::correction::compute(
		body &b1, body &b2, vec3 r1, vec3 r2, vec3 dir, scalar c
	) {
		correction result = uninitialized;
		result.body1 = &b1;
		result.body2 = &b2;
		result.direction = dir;
		const vec3 n1 = b1.state.rotation.inverse().rotate(dir);
		const vec3 n2 = b2.state.rotation.inverse().rotate(dir);
		const vec3 rot1 = vec::cross(r1, n1);
		const vec3 rot2 = vec::cross(r2, n2);
		result.rotation1 = b1.properties.inverse_inertia * rot1;
		result.rotation2 = b2.properties.inverse_inertia * rot2;
		const scalar w1 = b1.properties.inverse_mass + vec::dot(rot1, result.rotation1);
		const scalar w2 = b2.properties.inverse_mass + vec::dot(rot2, result.rotation2);
		result.delta_lambda = -c / (w1 + w2);
		return result;
	}

	void body::correction::apply_position(scalar &lambda) const {
		lambda += delta_lambda;
		const vec3 p = direction * delta_lambda;
		body1->state.position += p * body1->properties.inverse_mass;
		body2->state.position -= p * body2->properties.inverse_mass;
		body1->state.rotation = quat::unsafe_normalize(
			body1->state.rotation + body1->state.rotation * quats::from_vector((0.5f * delta_lambda) * rotation1)
		);
		body2->state.rotation = quat::unsafe_normalize(
			body2->state.rotation - body2->state.rotation * quats::from_vector((0.5f * delta_lambda) * rotation2)
		);
	}

	void body::correction::apply_velocity(scalar mag) const {
		const scalar p_norm = -mag * delta_lambda;
		const vec3 p = direction * p_norm;
		body1->state.linear_velocity += p * body1->properties.inverse_mass;
		body2->state.linear_velocity -= p * body2->properties.inverse_mass;
		body1->state.angular_velocity +=
			p_norm * body1->state.rotation.rotate(rotation1);
		body2->state.angular_velocity -=
			p_norm * body2->state.rotation.rotate(rotation2);
	}
}
