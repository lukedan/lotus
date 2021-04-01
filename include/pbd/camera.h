#pragma once

/// \file
/// Cameras.

#include "math/vector.h"

namespace pbd {
	/// Parameters of a camera, used to compute view and projection matrices.
	struct camera_parameters {
	public:
		/// No initialization.
		camera_parameters(uninitialized_t) {
		}

		cvec3d_t
			position = uninitialized, ///< The position of this camera.
			look_at = uninitialized, ///< The direction this camera points to.
			world_up = uninitialized; ///< The general upwards direction.
		double
			near_plane, ///< Distance to the near depth plane.
			far_plane, ///< Distance to the far depth plane.
			fov_y_radians, ///< Vertical field of view, in radians.
			aspect_ratio; ///< Aspect ratio.

		/// Creates a new \ref camera_parameters object.
		[[nodiscard]] inline constexpr static camera_parameters create_look_at(
			cvec3d_t at, cvec3d_t from_pos, cvec3d_t up = cvec3d::create({ 0.0, 0.0, 1.0 }),
			double asp_ratio = 1.333, double fovy_rads = 1.0472, // 4:3, 60 degrees
			double near = 0.1, double far = 1000.0
		) {
			return camera_parameters(from_pos, at, up, near, far, fovy_rads, asp_ratio);
		}
	protected:
		/// Initializes all fields of this struct.
		constexpr camera_parameters(
			cvec3d_t pos, cvec3d_t lookat, cvec3d_t up,
			double near, double far, double fovy, double asp_ratio
		) :
			position(pos), look_at(lookat), world_up(up),
			near_plane(near), far_plane(far), fov_y_radians(fovy), aspect_ratio(asp_ratio) {
		}
	};

	/// Camera matrices and direction vectors.
	struct camera {
	public:
		/// No initialization.
		camera(uninitialized_t) {
		}

		mat44d
			view_matrix = uninitialized, ///< Transforms object from world space to camera space.
			projection_matrix = uninitialized, ///< Projects objects from camera space onto a 2D plane.
			projection_view_matrix = uninitialized, ///< Product of \ref projection_matrix and \ref view_matrix.
			inverse_view_matrix = uninitialized; ///< Inverse of \ref view_matrix.
		cvec3d_t
			unit_forward = uninitialized, ///< Unit vector corresponding to the forward direction.
			unit_right = uninitialized, ///< Unit vector corresponding to the right direction.
			unit_up = uninitialized; ///< Unit vector corresponding to the up direction.

		/// Computes the \ref camera object that corresponds to the given \ref camera_parameters.
		[[nodiscard]] inline /*constexpr*/ static camera from_parameters(const camera_parameters &param) {
			cvec3d_t unit_forward = vec::unsafe_normalize(param.look_at - param.position);
			cvec3d_t unit_right = vec::unsafe_normalize(vec::cross(unit_forward, param.world_up));
			cvec3d_t unit_up = vec::cross(unit_right, unit_forward);

			mat33d rotation = matd::concat_rows(
				unit_right.transposed(), unit_up.transposed(), unit_forward.transposed()
			);
			cvec3d_t offset = -(rotation * param.position);

			mat44d view = zero;
			view.set_block(0, 0, rotation);
			view.set_block(0, 3, offset);
			view(3, 3) = 1.0;

			double f = 1.0 / std::tan(0.5 * param.fov_y_radians);
			mat44d projection = zero;
			projection(0, 0) = f / param.aspect_ratio;
			projection(1, 1) = f;
			projection(2, 2) = -param.far_plane / (param.near_plane - param.far_plane);
			projection(2, 3) = param.near_plane * param.far_plane / (param.near_plane - param.far_plane);
			projection(3, 2) = 1.0;

			return camera(unit_forward, unit_right, unit_up, view, projection);
		}
	protected:
		/// Initializes all fields of this struct.
		constexpr camera(
			cvec3d_t forward, cvec3d_t right, cvec3d_t up, mat44d view, mat44d proj
		) :
			view_matrix(view), projection_matrix(proj),
			projection_view_matrix(proj * view), inverse_view_matrix(mat44d::identity()), // TODO mat::invert()
			unit_forward(forward), unit_right(right), unit_up(up) {
		}
	};

}
