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
		[[nodiscard]] static body create(
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

		/// Applies an impulse to this body at the given position relative to its center of mass by immediately
		/// modifying its velocities.
		void apply_impulse_immediate(vec3 relative_pos, vec3 impulse_ws) {
			state.velocity.linear += impulse_ws * properties.inverse_mass;
			state.velocity.angular +=
				state.position.orientation.rotate(
					properties.inverse_inertia * state.position.orientation.inverse().rotate(
						vec::cross(relative_pos, impulse_ws)
					)
				);
		}

		/// Performs explicit time integration on body velocity. This function does not update previous state.
		void velocity_integration(scalar dt, vec3 external_accel, vec3 external_angular_accel) {
			if (properties.inverse_mass > 0.0f) {
				state.velocity.linear += dt * external_accel;
				state.velocity.angular += dt * external_angular_accel;
			}
		}
		/// Performs explicit time integration on body position. This function does not update previous state.
		void position_integration(scalar dt) {
			state.position.position += dt * state.velocity.linear;
			state.position.orientation = quatu::normalize(
				state.position.orientation +
				0.5f * dt * quat::from_vec3_xyz(state.velocity.angular) * state.position.orientation
			);
		}

		/// Returns the inertia tensor rotated to world coordinates.
		[[nodiscard]] mat33s get_rotated_inverse_inertia() const {
			const mat33s rot = state.position.orientation.into_rotation_matrix();
			return rot * properties.inverse_inertia * rot.transposed();
		}

		collision::shape *body_shape; ///< The shape of this body.
		material_properties material = uninitialized; ///< The material of this body.
		body_properties properties = uninitialized; ///< The properties of this body.
		body_state state = uninitialized; ///< The state of this body.
		body_state prev_state = uninitialized; ///< Body state after the previous timestep.
		vec3 applied_impulse = zero; ///< Impulse applied externally.
		vec3 applied_torque = zero; ///< Torque applied externally.
		void *user_data; ///< User data.
	};
	/// Data associated with a single particle.
	struct particle {
		/// No initialization.
		particle(uninitialized_t) {
		}
		/// Creates a new particle.
		[[nodiscard]] static particle create(particle_properties props, particle_state st) {
			particle result = uninitialized;
			result.properties    = props;
			result.state         = st;
			result.prev_position = result.state.position;
			return result;
		}

		particle_properties properties = uninitialized; ///< The mass of this particle.
		particle_state state = uninitialized; ///< The state of this particle.
		vec3 prev_position = uninitialized; ///< Position in the previous timestep.
	};
	/// Data associated with a single orientation.
	struct orientation {
		/// No initialization.
		orientation(uninitialized_t) {
		}

		// TODO move this to a separate struct?
		scalar inv_inertia; ///< Inverse inertia.
		orientation_state state = uninitialized; ///< The state of this orientation.
		uquats prev_orientation = uninitialized; ///< State in the previous timestep.
	};
}
