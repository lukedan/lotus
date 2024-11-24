#pragma once

/// \file
/// Rigid bodies and particles.

#include "lotus/collision/shape.h"
#include "body_properties.h"

namespace lotus::physics {
	/// Data associated with a single body.
	struct body {
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
		body(uninitialized_t) {
		}
		/// Creates a new body.
		[[nodiscard]] inline static body create(
			collision::shape &shape, material_properties mat, body_properties prop, body_state st
		) {
			body result = uninitialized;
			result.body_shape = &shape;
			result.material   = mat;
			result.properties = prop;
			result.state      = st;
			result.user_data  = nullptr;
			return result;
		}

		collision::shape *body_shape; ///< The shape of this body.
		material_properties material = uninitialized; ///< The material of this body.
		body_properties properties = uninitialized; ///< The properties of this body.
		body_state state = uninitialized; ///< The state of this body.
		vec3 prev_position = uninitialized; ///< Position after the previous timestep.
		uquats prev_rotation = uninitialized; ///< Rotation after the previous timestep.
		vec3 prev_linear_velocity = uninitialized; ///< Linear velocity after the previous timestep.
		vec3 prev_angular_velocity = uninitialized; ///< Angular velocity after the previous timestep.
		void *user_data; ///< User data.
	};
	/// Data associated with a single particle.
	struct particle {
		/// No initialization.
		particle(uninitialized_t) {
		}
		/// Creates a new particle.
		[[nodiscard]] inline static particle create(particle_properties props, particle_state st) {
			particle result = uninitialized;
			result.properties = props;
			result.state      = st;
			return result;
		}

		particle_properties properties = uninitialized; ///< The mass of this particle.
		particle_state state = uninitialized; ///< The state of this particle.
		vec3 prev_position = uninitialized; ///< Position in the previous timestep.
	};
}
