#pragma once

/// \file
/// Common physics engine definitions.

#include "lotus/math/vector.h"
#include "lotus/math/quaternion.h"

namespace lotus::physics {
	/// Type definitions for the physics engine.
	inline namespace types {
		using scalar = float; ///< Scalar type.
		using vec3 = cvec3<scalar>; ///< Vector type.
		using quats = quaternion<scalar>; ///< Quaternion type.
		using uquats = unit_quaternion<scalar>; ///< Unit quaternion type.
		using mat33s = mat33<scalar>; ///< 3x3 matrix type.
	}
}
