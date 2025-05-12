#pragma once

/// \file
/// Rigid bodies and particles.

#include "lotus/collision/shape.h"
#include "body_properties.h"

namespace lotus::physics {
	/// Data associated with a single body.
	struct body {
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
		body_position prev_position = uninitialized; ///< Position after the previous timestep.
		body_velocity prev_velocity = uninitialized; ///< Velocity after the previous timestep.
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
