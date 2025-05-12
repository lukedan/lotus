#pragma once

/// \file
/// Contact constraints.

#include "lotus/physics/common.h"
#include "lotus/physics/body.h"

namespace lotus::physics::xpbd::constraints {
	/// A contact constraint between two bodies.
	struct body_contact {
		/// Data associated with a correction.
		struct correction {
			/// No initialization.
			correction(uninitialized_t) {
			}
			/// Computes correction data but does not actually apply it. The offsets are in local space, while the
			/// direction is in world space and should be normalized.
			[[nodiscard]] static correction compute(
				body&, body&, vec3 r1, vec3 r2, vec3 dir, scalar c
			);
			/// \ref compute() with the raw offset.
			[[nodiscard]] inline static correction compute(
				body &b1, body &b2, vec3 r1, vec3 r2, vec3 delta_x
			) {
				const scalar norm = delta_x.norm();
				return compute(b1, b2, r1, r2, delta_x / norm, norm);
			}

			/// Applies this correction as a positional correction.
			void apply_position(scalar &lambda) const;
			/// Applies this correction as a velocity correction. The input magnitude is the real magnitude of the
			/// velocity change; this object should have been computed with a magnitude of 1.
			void apply_velocity(scalar mag) const;

			body *body1; ///< The first body.
			body *body2; ///< The second body.
			scalar delta_lambda; ///< The change in the multiplier.
			vec3 direction = uninitialized; ///< Normalized direction of the correction.
			vec3 rotation1 = uninitialized; ///< Partial rotation delta for the first body.
			vec3 rotation2 = uninitialized; ///< Partial rotation delta for the second body.
		};

		/// No initialization.
		body_contact(uninitialized_t) {
		}
		/// Creates a contact for the given bodies at the given contact position in local space.
		[[nodiscard]] inline static body_contact create_for(body &b1, body &b2, vec3 p1, vec3 p2, vec3 n) {
			body_contact result = uninitialized;
			result.offset1 = p1;
			result.offset2 = p2;
			result.normal = n;
			result.body1 = &b1;
			result.body2 = &b2;
			return result;
		}

		/// Projects this constraint.
		void project(scalar &lambda_n, scalar &lambda_t) {
			{ // handle penetration
				const vec3 global_contact1 = body1->state.position.local_to_global(offset1);
				const vec3 global_contact2 = body2->state.position.local_to_global(offset2);
				const scalar depth = vec::dot(global_contact1 - global_contact2, normal);
				if (depth < 0.0f) {
					return;
				}
				correction::compute(
					*body1, *body2, offset1, offset2, normal, depth
				).apply_position(lambda_n);
			}

			{ // handle static friction
				const vec3 global_contact1 = body1->state.position.local_to_global(offset1);
				const vec3 old_global_contact1 = body1->prev_position.local_to_global(offset1);
				const vec3 global_contact2 = body2->state.position.local_to_global(offset2);
				const vec3 old_global_contact2 = body2->prev_position.local_to_global(offset2);

				const vec3 delta_p = (global_contact1 - old_global_contact1) - (global_contact2 - old_global_contact2);
				const vec3 delta_pt = delta_p - normal * vec::dot(normal, delta_p);

				const scalar static_friction = std::min(body1->material.static_friction, body2->material.static_friction);

				const auto cor = correction::compute(
					*body1, *body2, offset1, offset2, delta_pt
				);
				const scalar max_multiplier = static_friction * lambda_n;
				if (cor.delta_lambda > max_multiplier) {
					cor.apply_position(lambda_t);
				}
			}
		}

		/// Offset of the spring's connection to \ref body1 in its local coordinates.
		vec3 offset1 = uninitialized;
		/// Offset of the spring's connection to \ref body2 in its local coordinates.
		vec3 offset2 = uninitialized;
		vec3 normal = uninitialized; ///< Contact normal.
		body *body1; ///< The first body.
		body *body2; ///< The second body.
	};
}
