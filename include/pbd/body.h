#pragma once

/// \file
/// Rigid bodies and particles.

#include "shapes/shape.h"
#include "body_properties.h"

namespace pbd {
	/// Data associated with a single body.
	struct body {
		/// No initialization.
		body(uninitialized_t) {
		}
		/// Creates a new body.
		[[nodiscard]] inline static body create(shape &shape, body_properties prop, body_state st) {
			body result = uninitialized;
			result.body_shape = &shape;
			result.properties = prop;
			result.state = st;
			result.user_data = nullptr;
			return result;
		}

		shape *body_shape; ///< The shape of this body.
		body_properties properties = uninitialized; ///< The properties of this body.
		body_state state = uninitialized; ///< The state of this body.
		cvec3d prev_position = uninitialized; ///< Position in the previous timestep.
		uquatd prev_rotation = uninitialized; ///< Rotation in the previous timestep.
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
			result.state = st;
			return result;
		}

		particle_properties properties = uninitialized; ///< The mass of this particle.
		particle_state state = uninitialized; ///< The state of this particle.
		cvec3d prev_position = uninitialized; ///< Position in the previous timestep.
	};
}
