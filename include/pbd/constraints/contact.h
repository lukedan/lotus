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
		void project(double &lambda_n, double &lambda_t) {
			{ // handle penetration
				cvec3d global_contact1 = body1->state.position + body1->state.rotation.rotate(offset1);
				cvec3d global_contact2 = body2->state.position + body2->state.rotation.rotate(offset2);
				double depth = vec::dot(global_contact1 - global_contact2, normal);
				if (depth < 0.0) {
					return;
				}
				body::correction::compute(
					*body1, *body2, offset1, offset2, normal, depth
				).apply_position(lambda_n);
			}

			{ // handle static friction
				cvec3d global_contact1 = body1->state.position + body1->state.rotation.rotate(offset1);
				cvec3d old_global_contact1 = body1->prev_position + body1->prev_rotation.rotate(offset1);
				cvec3d global_contact2 = body2->state.position + body2->state.rotation.rotate(offset2);
				cvec3d old_global_contact2 = body2->prev_position + body2->prev_rotation.rotate(offset2);

				cvec3d delta_p = (global_contact1 - old_global_contact1) - (global_contact2 - old_global_contact2);
				cvec3d delta_pt = delta_p - normal * vec::dot(normal, delta_p);

				double static_friction = std::min(body1->material.static_friction, body2->material.static_friction);

				auto correction = body::correction::compute(
					*body1, *body2, offset1, offset2, delta_pt
				);
				double max_multiplier = static_friction * lambda_n;
				if (correction.delta_lambda > max_multiplier) {
					correction.apply_position(lambda_t);
				}
			}
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
