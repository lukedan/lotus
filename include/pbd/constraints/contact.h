#pragma once

/// \file
/// Contact constraints.

namespace pbd::constraints {
	/// A contact constraint between two bodies.
	struct body_contact {
		/// No initialization.
		body_contact(uninitialized_t) {
		}
		/// Creates a contact for the given bodies at the given contact position in local space.
		[[nodiscard]] inline static body_contact create_for(body &b1, body &b2, cvec3d p1, cvec3d p2, cvec3d n) {
			body_contact result = uninitialized;
			result.offset1 = p1;
			result.offset2 = p2;
			result.normal = n;
			result.body1 = &b1;
			result.body2 = &b2;
			return result;
		}

		/// Projects this constraint.
		void project(double inv_dt2, double &lambda) {
			cvec3d global_contact1 = body1->state.position + body1->state.rotation.rotate(offset1);
			cvec3d global_contact2 = body2->state.position + body2->state.rotation.rotate(offset2);
			double depth = vec::dot(global_contact1 - global_contact2, normal);
			if (depth < 0.0) {
				return;
			}

			cvec3d n1 = body1->state.rotation.inverse().rotate(normal);
			cvec3d n2 = body2->state.rotation.inverse().rotate(normal);
			cvec3d rot1 = vec::cross(offset1, n1);
			cvec3d rot2 = vec::cross(offset2, n2);
			cvec3d inertia_rot1 = body1->properties.inverse_inertia * rot1;
			cvec3d inertia_rot2 = body2->properties.inverse_inertia * rot2;
			double w1 = body1->properties.inverse_mass + vec::dot(rot1, inertia_rot1);
			double w2 = body2->properties.inverse_mass + vec::dot(rot2, inertia_rot2);

			double delta_lambda = -depth / (w1 + w2);
			lambda += delta_lambda;
			cvec3d p = normal * delta_lambda;
			body1->state.position += p * body1->properties.inverse_mass;
			body2->state.position -= p * body2->properties.inverse_mass;
			body1->state.rotation = quat::unsafe_normalize(
				body1->state.rotation +
				(0.5 * delta_lambda) * body1->state.rotation * quatd::from_vector(inertia_rot1)
			);
			body2->state.rotation = quat::unsafe_normalize(
				body2->state.rotation -
				(0.5 * delta_lambda) * body2->state.rotation * quatd::from_vector(inertia_rot2)
			);
		}

		/// Offset of the spring's connection to \ref body1 in its local coordinates.
		cvec3d offset1 = uninitialized;
		/// Offset of the spring's connection to \ref body2 in its local coordinates.
		cvec3d offset2 = uninitialized;
		cvec3d normal = uninitialized; ///< Contact normal.
		body *body1; ///< The first body.
		body *body2; ///< The second body.
	};
}
