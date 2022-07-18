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
			typename T, typename RealType = T, std::size_t Alignment = sizeof(T)
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
		/// Vector type used in shaders.
		template <typename T, std::size_t Dim> struct alignas(T) vector {
		public:
			/// Correponding C++ type that this type provides implicit conversions from and to.
			using cpp_type = column_vector<Dim, typename T::cpp_type>;

			/// No initialization.
			vector() = default;
			/// Implicit conversion from \ref column_vector.
			constexpr vector(const cpp_type &v) {
				for (std::size_t i = 0; i < Dim; ++i) {
					_value[i] = v[i];
				}
			}
			/// Implicit conversion to \ref column_vector.
			[[nodiscard]] operator cpp_type() const {
				cpp_type result = uninitialized;
				for (std::size_t i = 0; i < Dim; ++i) {
					result[i] = _value[i];
				}
				return result;
			}
		private:
			T _value[Dim]; ///< Vector data.
		};
		/// Matrix type used in shaders.
		template <typename T, std::size_t Rows, std::size_t Cols> struct alignas(T) row_major_matrix {
		public:
			/// Correponding C++ type that this type provides implicit conversions from and to.
			using cpp_type = matrix<Rows, Cols, typename T::cpp_type>;

			/// No initialization.
			row_major_matrix() = default;
			/// Implicit conversion from \ref matrix.
			constexpr row_major_matrix(const cpp_type &m) {
				for (std::size_t r = 0; r < Rows; ++r) {
					for (std::size_t c = 0; c < Cols; ++c) {
						_value[r][c] = m(r, c);
					}
				}
			}
			/// Implicit conversion to \ref matrix.
			[[nodiscard]] operator cpp_type() const {
				cpp_type result = uninitialized;
				for (std::size_t r = 0; r < Rows; ++r) {
					for (std::size_t c = 0; c < Cols; ++c) {
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
	template <typename T, std::size_t Dim> using vector = _details::vector<T, Dim>;
	/// Assumes matrices are row-major.
	template <typename T, std::size_t Rows, std::size_t Cols> using matrix =
		_details::row_major_matrix<T, Rows, Cols>;


	using bool_    = _details::primitive<std::uint32_t, bool>;

	using int_     = _details::primitive<std::int32_t>;
	using int64_t  = _details::primitive<std::int64_t>;
	using uint     = _details::primitive<std::uint32_t>;
	using uint64_t = _details::primitive<std::uint64_t>;
	using dword    = uint;

	using half     = _details::primitive<std::uint16_t>; // TODO float16 type
	using float_   = _details::primitive<float>;
	using double_  = _details::primitive<double>;
	static_assert(sizeof(float) == sizeof(std::uint32_t), "Expecting float to be 32 bits");
	static_assert(sizeof(double) == sizeof(std::uint64_t), "Expecting double to be 64 bits");


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
