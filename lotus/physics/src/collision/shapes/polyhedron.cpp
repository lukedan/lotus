#include "lotus/collision/shapes/polyhedron.h"

/// \file
/// Implementation of polyhedron-related functions.

namespace lotus::collision::shapes {
	polyhedron::properties polyhedron::properties::compute_for(
		std::span<const vec3> verts, std::span<const std::array<std::uint32_t, 3>> faces
	) {
		constexpr mat33s _canonical{
			{ 1.0f / 60.0f,  1.0f / 120.0f, 1.0f / 120.0f },
			{ 1.0f / 120.0f, 1.0f / 60.0f,  1.0f / 120.0f },
			{ 1.0f / 120.0f, 1.0f / 120.0f, 1.0f / 60.0f  }
		};

		polyhedron::properties result = uninitialized;
		result.center_of_mass    = zero;
		result.covariance_matrix = zero;
		result.volume            = zero;
		for (const auto &f : faces) {
			vec3 p1 = verts[f[0]];
			vec3 p2 = verts[f[1]];
			vec3 p3 = verts[f[2]];
			mat33s a = mat::concat_columns(p1, p2, p3);
			scalar det = mat::lup_decompose(a).determinant();
			result.covariance_matrix += det * mat::multiply_into_symmetric(a, _canonical * a.transposed());
			result.volume            += det;
			result.center_of_mass    += (det * 0.25f) * (p1 + p2 + p3);
		}
		result.center_of_mass /= result.volume;
		result.volume         /= 6.0f;
		return result;
	}
}
