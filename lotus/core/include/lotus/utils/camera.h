#pragma once

/// \file
/// Cameras.

#include "lotus/math/matrix.h"
#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"

namespace lotus {
	template <typename> struct camera_parameters;

	/// Camera matrices and direction vectors.
	template <typename T> struct camera {
		friend camera camera_parameters<T>::into_camera() const;
	public:
		/// No initialization.
		camera(uninitialized_t) {
		}

		mat44<T> view_matrix            = uninitialized; ///< Transforms objects from world space to camera space.
		mat44<T> projection_matrix      = uninitialized; ///< Projects objects from camera space onto a 2D plane.
		mat44<T> projection_view_matrix = uninitialized; ///< Product of \ref projection_matrix and \ref view_matrix.
		mat44<T> inverse_view_matrix    = uninitialized; ///< Inverse of \ref view_matrix.
		cvec3<T> unit_forward = uninitialized; ///< Unit vector corresponding to the forward direction.
		cvec3<T> unit_right   = uninitialized; ///< Unit vector corresponding to the right direction.
		cvec3<T> unit_up      = uninitialized; ///< Unit vector corresponding to the up direction.
	protected:
		/// Initializes all fields of this struct.
		constexpr camera(cvec3<T> forward, cvec3<T> right, cvec3<T> up, mat44<T> view, mat44<T> proj) :
			view_matrix(view), projection_matrix(proj),
			projection_view_matrix(proj * view), inverse_view_matrix(view.inverse()),
			unit_forward(forward), unit_right(right), unit_up(up) {
		}
	};

	/// Parameters of a camera, used to compute view and projection matrices.
	template <typename T> struct camera_parameters {
	public:
		/// No initialization.
		camera_parameters(uninitialized_t) {
		}

		cvec3<T> position = uninitialized; ///< The position of this camera.
		cvec3<T> look_at  = uninitialized; ///< The direction this camera points to.
		cvec3<T> world_up = uninitialized; ///< The general upwards direction.
		T near_plane; ///< Distance to the near depth plane.
		T far_plane;  ///< Distance to the far depth plane.
		T fov_y_radians; ///< Vertical field of view, in radians.
		T aspect_ratio;  ///< Aspect ratio.

		/// Creates a new \ref camera_parameters object.
		[[nodiscard]] inline constexpr static camera_parameters create_look_at(
			cvec3<T> at, cvec3<T> from_pos, cvec3<T> up = { 0.0f, 1.0f, 0.0f },
			T asp_ratio = 1.333f, T fovy_rads = 1.0472f, // 4:3, 60 degrees
			T near_clip = 0.1f, T far_clip = 1000.0f
		) {
			return camera_parameters(from_pos, at, up, near_clip, far_clip, fovy_rads, asp_ratio);
		}

		/// Creates a camera that correspond to this set of parameters, using reversed depth.
		[[nodiscard]] /*constexpr*/ camera<T> into_camera() const {
			cvec3<T> unit_forward = vec::unsafe_normalize(look_at - position);
			cvec3<T> unit_right = vec::unsafe_normalize(vec::cross(unit_forward, world_up));
			cvec3<T> unit_up = vec::cross(unit_right, unit_forward);

			mat33<T> rotation = mat<T>::concat_columns(unit_right, unit_up, unit_forward).transposed();
			cvec3<T> offset = -(rotation * position);

			mat44<T> view = zero;
			view.set_block(0, 0, rotation);
			view.set_block(0, 3, offset);
			view(3, 3) = 1.0f;

			T f = 1.0f / std::tan(0.5f * fov_y_radians);
			mat44<T> projection = zero;
			projection(0, 0) = f / aspect_ratio;
			projection(1, 1) = f;
			projection(2, 2) = -near_plane / (far_plane - near_plane);
			projection(2, 3) = near_plane * far_plane / (far_plane - near_plane);
			projection(3, 2) = 1.0f;

			return camera(unit_forward, unit_right, unit_up, view, projection);
		}

		/// Rotates the camera around the \ref world_up axis, which may be negated to ensure that the behavior is
		/// desirable when the camera is inverted vertically.
		constexpr void rotate_around_world_up(cvec2<T> angles) {
			auto offset = position - look_at;

			if (world_up[0] + world_up[1] + world_up[2] < 0) {
				angles[0] = -angles[0];
			}
			auto rot_x = quat::from_axis_angle(world_up, angles[0]);
			offset = rot_x.rotate(offset);

			auto rot_y = quat::from_axis_angle(vec::cross(offset, world_up), angles[1]);
			auto new_offset = rot_y.rotate(offset);
			if (vec::dot(vec::cross(new_offset, world_up), vec::cross(offset, world_up)) < 0.0f) {
				world_up = -world_up; // over top and flip
			}
			offset = new_offset;

			position = look_at + offset;
		}
	protected:
		/// Initializes all fields of this struct.
		constexpr camera_parameters(
			cvec3<T> pos, cvec3<T> lookat, cvec3<T> up, T near_clip, T far_clip, T fovy, T asp_ratio
		) :
			position(pos), look_at(lookat), world_up(up),
			near_plane(near_clip), far_plane(far_clip), fov_y_radians(fovy), aspect_ratio(asp_ratio) {
		}
	};
}
