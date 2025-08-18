#pragma once

/// \file
/// Implementatino of shader types.

#include <cstddef>

#include "lotus/math/vector.h"
#include "lotus/math/matrix.h"

namespace lotus::renderer::shader_types {
	namespace _details {
		/// A primitive shader type.
		template <
			typename T, typename RealType = T, usize Alignment = sizeof(T)
		> struct alignas(Alignment) primitive {
		public:
			/// Correponding C++ type that this type provides implicit conversions from and to.
			using cpp_type = RealType;

			/// Default constructor.
			primitive() = default;
			/// Implicit conversion from the real type.
			constexpr primitive(const RealType &val) : _value(static_cast<T>(val)) {
			}
			/// Implicit conversion to the real type.
			[[nodiscard]] operator RealType() const {
				return static_cast<RealType>(_value);
			}
		private:
			T _value; ///< Value.
		};

		/// Used to obtain the underlying type of a scalar shader type.
		template <typename T> struct scalar_type_properties {
			using storage_type = T; ///< The type used for storage.
			using real_type = T; ///< The actual type.
		};
		/// Specialization for wrapped types.
		template <
			typename U, typename T, usize Alignment
		> struct scalar_type_properties<primitive<U, T, Alignment>> {
			using storage_type = T; ///< The type used for storage.
			using real_type = U; ///< The actual type.
		};

		/// Vector type used in shaders.
		template <typename T, usize Dim> struct alignas(T) vector {
		public:
			/// Correponding C++ type that this type provides implicit conversions from and to.
			using cpp_type = column_vector<Dim, typename scalar_type_properties<T>::real_type>;

			/// No initialization.
			vector() = default;
			/// Implicit conversion from \ref column_vector.
			constexpr vector(const cpp_type &v) {
				for (usize i = 0; i < Dim; ++i) {
					_value[i] = v[i];
				}
			}
			/// Implicit conversion to \ref column_vector.
			[[nodiscard]] operator cpp_type() const {
				cpp_type result = uninitialized;
				for (usize i = 0; i < Dim; ++i) {
					result[i] = _value[i];
				}
				return result;
			}
		private:
			T _value[Dim]; ///< Vector data.
		};

		/// Matrix type used in shaders.
		template <typename T, usize Rows, usize Cols> struct alignas(T) row_major_matrix {
		public:
			/// Correponding C++ type that this type provides implicit conversions from and to.
			using cpp_type = matrix<Rows, Cols, typename scalar_type_properties<T>::real_type>;

			/// No initialization.
			row_major_matrix() = default;
			/// Implicit conversion from \ref matrix.
			constexpr row_major_matrix(const cpp_type &m) {
				for (usize r = 0; r < Rows; ++r) {
					for (usize c = 0; c < Cols; ++c) {
						_value[r][c] = m(r, c);
					}
				}
			}
			/// Implicit conversion to \ref matrix.
			[[nodiscard]] operator cpp_type() const {
				cpp_type result = uninitialized;
				for (usize r = 0; r < Rows; ++r) {
					for (usize c = 0; c < Cols; ++c) {
						result(r, c) = _value[r][c];
					}
				}
				return result;
			}
		private:
			T _value[Rows][Cols]; ///< Matrix data.
		};
	}

	/// Vector type.
	template <typename T, usize Dim> using vector = _details::vector<T, Dim>;
	/// Assumes matrices are row-major.
	template <typename T, usize Rows, usize Cols> using matrix =
		_details::row_major_matrix<T, Rows, Cols>;


	using bool_    = _details::primitive<u32, bool>;

	using int_     = i32;
	using int64_t  = i64;
	using uint     = u32;
	using uint64_t = u64;
	using dword    = uint;

	using half     = u16; // TODO float16 type
	using float_   = f32;
	using double_  = f64;


	using int2     = vector<int_, 2>;
	using int3     = vector<int_, 3>;
	using int4     = vector<int_, 4>;

	using uint2    = vector<uint, 2>;
	using uint3    = vector<uint, 3>;
	using uint4    = vector<uint, 4>;

	using float2   = vector<float_, 2>;
	using float3   = vector<float_, 3>;
	using float4   = vector<float_, 4>;


	using float1x2 = matrix<float_, 1, 2>;
	using float1x3 = matrix<float_, 1, 3>;
	using float1x4 = matrix<float_, 1, 4>;

	using float2x1 = matrix<float_, 2, 1>;
	using float2x2 = matrix<float_, 2, 2>;
	using float2x3 = matrix<float_, 2, 3>;
	using float2x4 = matrix<float_, 2, 4>;

	using float3x1 = matrix<float_, 3, 1>;
	using float3x2 = matrix<float_, 3, 2>;
	using float3x3 = matrix<float_, 3, 3>;
	using float3x4 = matrix<float_, 3, 4>;

	using float4x1 = matrix<float_, 4, 1>;
	using float4x2 = matrix<float_, 4, 2>;
	using float4x3 = matrix<float_, 4, 3>;
	using float4x4 = matrix<float_, 4, 4>;
}
