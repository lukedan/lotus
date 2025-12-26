#pragma once

/// \file
/// Classes related to tangent frames.

#include "vector.h"

namespace lotus {
	/// A tangent frame.
	template <typename T> struct tangent_frame {
		using scalar_type = T; ///< Scalar type.
		using vector_type = cvec3<scalar_type>; ///< Vector type.

		/// No initialization.
		tangent_frame(uninitialized_t) {
		}
		/// Creates a new tangent frame from the given normal, tangent, and bitangent.
		static tangent_frame from_ntb(vector_type n, vector_type t, vector_type b) {
			tangent_frame result = uninitialized;
			result.normal    = n;
			result.tangent   = t;
			result.bitangent = b;
			return result;
		}
		/// Constructs an arbitrary orthonormal tangent frame from the given normal vector where
		/// <tt>tangent x bitangent = normal</tt>, assuming that the normal vector is normalized.
		///
		/// \sa https://graphics.pixar.com/library/OrthonormalB/paper.pdf
		static tangent_frame from_normal(vector_type n) {
			const scalar_type sign = n[2] > 0.0f ? 1.0f : -1.0f;
			const scalar_type a = -1.0f / (sign + n[2]);
			const scalar_type b = n[0] * n[1] * a;
			return tangent_frame::from_ntb(
				n,
				vector_type(1.0f + sign * n[0] * n[0] * a, sign * b, -sign * n[0]),
				vector_type(b, sign + n[1] * n[1] * a, -n[1])
			);
		}

		vector_type normal    = uninitialized;
		vector_type tangent   = uninitialized;
		vector_type bitangent = uninitialized;

		/// Returns a matrix containing the basis vectors as columns. The matrix converts a vector from tangent space
		/// to world space when multiplied with a column vector.
		[[nodiscard]] mat33<T> get_tangent_to_world_matrix() const {
			return mat::concat_columns(normal, tangent, bitangent);
		}
		/// Returns a matrix containing the basis vectors as rows. This matrix converts a vector from world space to
		/// tangent space when multiplied with a column vector.
		[[nodiscard]] mat33<T> get_world_to_tangent_matrix() const {
			return mat::concat_columns(normal, tangent, bitangent).transposed();
		}
	};
}
