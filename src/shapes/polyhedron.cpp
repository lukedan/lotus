#include "pbd/shapes/polyhedron.h"

/// \file
/// Implementation of polyhedron-related functions.

namespace pbd::shapes {
	polyhedron::properties polyhedron::properties::compute_for(
		const std::vector<cvec3d> &verts, const std::vector<std::array<std::size_t, 3>> &faces
	) {
		constexpr mat33d _canonical{
			{ 1.0 / 60.0,  1.0 / 120.0, 1.0 / 120.0 },
			{ 1.0 / 120.0, 1.0 / 60.0,  1.0 / 120.0 },
			{ 1.0 / 120.0, 1.0 / 120.0, 1.0 / 60.0  }
		};

		polyhedron::properties result = uninitialized;
		result.center_of_mass = zero;
		result.covariance_matrix = zero;
		result.volume = zero;
		for (const auto &f : faces) {
			auto p1 = verts[f[0]];
			auto p2 = verts[f[1]];
			auto p3 = verts[f[2]];
			mat33d a = matd::concat_columns(p1, p2, p3);
			double det = matd::lup_decompose(a).determinant();
			result.covariance_matrix += det * matd::multiply_into_symmetric(a, _canonical * a.transposed());
			result.volume += det;
			result.center_of_mass += (det * 0.25) * (p1 + p2 + p3);
		}
		result.center_of_mass /= result.volume;
		result.volume /= 6.0;
		return result;
	}
}
