#pragma once

/// \file
/// Quaternions.

#include <cassert>
#include <type_traits>

#include "lotus/math/vector.h"

namespace lotus {
	/// Whether this quaternion is a unit quaternion.
	enum class quaternion_kind {
		arbitrary, ///< Quaternion with arbitrary magnitude.
		unit ///< Unit quaternion with a magnitude of 1.
	};

	struct quat;

	/// A quaternion.
	template <typename T, quaternion_kind Kind = quaternion_kind::arbitrary> struct quaternion {
		friend quat;
		template <typename, quaternion_kind> friend struct quaternion;
	public:
		using value_type = T; ///< Value type.
		constexpr static quaternion_kind kind = Kind; ///< Whether this quaternion is a unit quaternion.

		/// Does not initialize the components.
		quaternion(uninitialized_t) {
		}
		/// Zero-initializes this quaternion.
		constexpr quaternion(zero_t) : quaternion(0, 0, 0, 0) {
		}
		/// Unit quaternions can be implicitly converted into arbitrary quaternions.
		template <
			typename Dummy = int,
			std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> constexpr quaternion(const quaternion<T, quaternion_kind::unit> &src) :
			quaternion(src.w(), src.x(), src.y(), src.z()) {
		}
		/// Default move constructor.
		constexpr quaternion(quaternion&&) = default;
		/// Default copy constructor.
		constexpr quaternion(const quaternion&) = default;
		/// Default move assignment.
		constexpr quaternion &operator=(quaternion&&) = default;
		/// Default copy assignment.
		constexpr quaternion &operator=(const quaternion&) = default;

		/// Creates a quaternion using the given elements.
		template <
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> [[nodiscard]] constexpr static quaternion from_wxyz(T w, T x, T y, T z) {
			return quaternion(std::move(w), std::move(x), std::move(y), std::move(z));
		}
		/// Returns the identity quaternion.
		[[nodiscard]] constexpr static quaternion<T, quaternion_kind::unit> identity() {
			return quaternion<T, quaternion_kind::unit>(1, 0, 0, 0);
		}
		/// Creates a quaternion using the given 3D vector for X, Y, and Z, leaving W empty.
		template <typename Vec> [[nodiscard]] constexpr static std::enable_if_t<
			Vec::dimensionality == 3 && Kind == quaternion_kind::arbitrary, quaternion
		> from_vector(const Vec &v) {
			return from_wxyz(static_cast<T>(0), v[0], v[1], v[2]);
		}


		/// Indexing.
		template <
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> [[nodiscard]] constexpr T &w() {
			return _w;
		}
		/// \overload
		[[nodiscard]] constexpr const T &w() const {
			return _w;
		}

		/// Indexing.
		template <
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> [[nodiscard]] constexpr T &x() {
			return _x;
		}
		/// \overload
		[[nodiscard]] constexpr const T &x() const {
			return _x;
		}

		/// Indexing.
		template <
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> [[nodiscard]] constexpr T &y() {
			return _y;
		}
		/// \overload
		[[nodiscard]] constexpr const T &y() const {
			return _y;
		}

		/// Indexing.
		template <
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> [[nodiscard]] constexpr T &z() {
			return _z;
		}
		/// \overload
		[[nodiscard]] constexpr const T &z() const {
			return _z;
		}


		// arithmetic
		/// In-place memberwise addition.
		template <
			quaternion_kind OtherKind,
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> constexpr quaternion &operator+=(const quaternion<T, OtherKind> &rhs) {
			w() += rhs.w();
			x() += rhs.x();
			y() += rhs.y();
			z() += rhs.z();
			return *this;
		}
		/// Memberwise addition.
		template <
			quaternion_kind OtherKind,
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> [[nodiscard]] friend constexpr quaternion operator+(
			quaternion<T, Kind> lhs, const quaternion<T, OtherKind> &rhs
		) {
			lhs += rhs;
			return lhs;
		}

		/// In-place memberwise subtraction.
		template <
			quaternion_kind OtherKind,
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> constexpr quaternion &operator-=(const quaternion<T, OtherKind> &rhs) {
			w() -= rhs.w();
			x() -= rhs.x();
			y() -= rhs.y();
			z() -= rhs.z();
			return *this;
		}
		/// Memberwise subtraction.
		template <
			quaternion_kind OtherKind,
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> [[nodiscard]] friend constexpr quaternion operator-(
			quaternion<T, Kind> lhs, const quaternion<T, OtherKind> &rhs
		) {
			lhs -= rhs;
			return lhs;
		}

		/// In-place scalar multiplication.
		template <
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> constexpr quaternion &operator*=(const T &rhs) {
			w() *= rhs;
			x() *= rhs;
			y() *= rhs;
			z() *= rhs;
			return *this;
		}
		/// Scalar multiplication.
		[[nodiscard]] friend constexpr quaternion<T, quaternion_kind::arbitrary> operator*(
			const quaternion<T, Kind> &lhs, const T &rhs
		) {
			quaternion<T, quaternion_kind::arbitrary> res = lhs;
			res *= rhs;
			return res;
		}
		/// Scalar multiplication.
		[[nodiscard]] friend constexpr quaternion<T, quaternion_kind::arbitrary> operator*(
			const T &lhs, const quaternion<T, Kind> &rhs
		) {
			quaternion<T, quaternion_kind::arbitrary> res = rhs;
			res *= lhs;
			return res;
		}

		/// In-place quaternion multiplication.
		template <quaternion_kind OtherKind> constexpr std::enable_if_t<
			Kind == quaternion_kind::arbitrary || OtherKind == quaternion_kind::unit, quaternion&
		> operator*=(const quaternion<T, OtherKind> &rhs) {
			T res_w = w() * rhs.w() - vec::dot(axis(), rhs.axis());
			auto res_axis = w() * rhs.axis() + rhs.w() * axis() + vec::cross(axis(), rhs.axis());
			_w = std::move(res_w);
			_x = std::move(res_axis[0]);
			_y = std::move(res_axis[1]);
			_z = std::move(res_axis[2]);
			return *this;
		}
		/// Quaternion multiplication.
		template <quaternion_kind OtherKind> [[nodiscard]] friend constexpr quaternion<
			T,
			Kind == quaternion_kind::arbitrary || OtherKind == quaternion_kind::arbitrary ?
				quaternion_kind::arbitrary : quaternion_kind::unit
		> operator*(const quaternion<T, Kind> &lhs, const quaternion<T, OtherKind> &rhs) {
			quaternion<
				T,
				Kind == quaternion_kind::arbitrary || OtherKind == quaternion_kind::arbitrary ?
					quaternion_kind::arbitrary : quaternion_kind::unit
			> result = lhs;
			result *= rhs;
			return result;
		}

		/// In-place scalar division.
		template <
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> constexpr quaternion &operator/=(const T &rhs) {
			w() /= rhs;
			x() /= rhs;
			y() /= rhs;
			z() /= rhs;
			return *this;
		}
		/// Scalar division.
		[[nodiscard]] friend constexpr quaternion<T, quaternion_kind::arbitrary> operator/(
			const quaternion<T, Kind> &lhs, const T &rhs
		) {
			quaternion<T, quaternion_kind::arbitrary> res = lhs;
			res /= rhs;
			return std::move(res);
		}


		// conversion
		/// Conversion to another floating-point data type.
		template <typename U> [[nodiscard]] constexpr std::enable_if_t<
			std::is_floating_point_v<U>, quaternion<U, Kind>
		> into() const {
			return quaternion<U, Kind>(
				static_cast<U>(_w), static_cast<U>(_x), static_cast<U>(_y), static_cast<U>(_z)
			);
		}


		// properties
		/// Returns the squared magnitude of this quaternion.
		[[nodiscard]] constexpr T squared_magnitude() const {
			if constexpr (Kind == quaternion_kind::unit) {
				return static_cast<T>(1);
			} else {
				return _x * _x + _y * _y + _z * _z + _w * _w;
			}
		}
		/// Returns the square root of \ref squared_magnitude().
		[[nodiscard]] constexpr T magnitude() const {
			if constexpr (Kind == quaternion_kind::unit) {
				return static_cast<T>(1);
			} else {
				return std::sqrt(squared_magnitude());
			}
		}

		/// Returns the rotation axis. This is unnormalized even for unit quaternions.
		[[nodiscard]] constexpr cvec3<T> axis() const {
			return { x(), y(), z() };
		}


		/// Returns the conjugate of this quaternion.
		[[nodiscard]] constexpr quaternion conjugate() const {
			return quaternion(_w, -_x, -_y, -_z);
		}
		/// Returns the inverse of this quaternion.
		[[nodiscard]] constexpr quaternion inverse() const {
			quaternion result = conjugate();
			if constexpr (Kind != quaternion_kind::unit) {
				result /= result.squared_magnitude();
			}
			return result;
		}

		/// Returns the corresponding rotation matrix.
		[[nodiscard]] constexpr matrix<3, 3, T> into_matrix() const {
			auto xx = x() * x();
			auto yy = y() * y();
			auto zz = z() * z();
			
			auto xy = x() * y();
			auto xz = x() * z();
			auto yz = y() * z();

			auto xw = x() * w();
			auto yw = y() * w();
			auto zw = z() * w();

			T s = static_cast<T>(2);
			if constexpr (Kind == quaternion_kind::arbitrary) {
				s /= squared_magnitude();
			}
			return {
				{ 1 - s * (yy + zz), s * (xy - zw),     s * (xz + yw)     },
				{ s * (xy + zw),     1 - s * (xx + zz), s * (yz - xw)     },
				{ s * (xz - yw),     s * (yz + xw),     1 - s * (xx + yy) }
			};
		}

		/// Rotates a vector.
		template <typename Vec> [[nodiscard]] constexpr std::enable_if_t<Vec::dimensionality == 3, Vec> rotate(
			const Vec &v1
		) const {
			T s = w();
			auto v = axis();
			auto result = (2 * vec::dot(v, v1)) * v + (s * s - v.squared_norm()) * v1 + (2 * s) * vec::cross(v, v1);
			if constexpr (Kind == quaternion_kind::arbitrary) {
				result /= squared_magnitude();
			}
			return result;
		}
	private:
		T
			_w, ///< The cosine of half the rotation angle.
			_x, ///< Rotation axis X times the sine of half the rotation angle.
			_y, ///< Rotation axis Y times the sine of half the rotation angle.
			_z; ///< Rotation axis Z times the sine of half the rotation angle.

		/// Initializes all components of this quaternion.
		constexpr quaternion(T cw, T cx, T cy, T cz) :
			_w(std::move(cw)), _x(std::move(cx)), _y(std::move(cy)), _z(std::move(cz)) {
		}
	};
	template <typename T> using unit_quaternion = quaternion<T, quaternion_kind::unit>; ///< Unit quaternions.

	using quatf = quaternion<float>; ///< Shorthand for quaternions of \p float.
	using quatd = quaternion<double>; ///< Shorthand for quaternions of \p double.

	using uquatf = unit_quaternion<float>; ///< Shorthand for unit quaternions of \p float.
	using uquatd = unit_quaternion<double>; ///< Shorthand for unit quaternions of \p double.


	/// Quaternion utilities.
	struct quat {
		/// Creates a quaternion from the given normalized axis and rotation angle.
		template <typename Vec> [[nodiscard]] inline static constexpr std::enable_if_t<
			Vec::dimensionality == 3, unit_quaternion<typename Vec::value_type>
		> from_normalized_axis_angle(const Vec &axis, typename Vec::value_type angle) {
			using _type = typename Vec::value_type;

			angle /= static_cast<_type>(2);
			_type w = std::cos(angle);
			_type sin_half = std::sin(angle);
			return unit_quaternion<_type>(w, sin_half * axis[0], sin_half * axis[1], sin_half * axis[2]);
		}
		/// Creates a quaternion from the given axis and rotation angle. This function calls
		/// \ref vec::unsafe_normalize() to normalize the rotation axis; use \ref 
		template <typename Vec> [[nodiscard]] inline static constexpr std::enable_if_t<
			Vec::dimensionality == 3, unit_quaternion<typename Vec::value_type>
		> from_axis_angle(const Vec &axis, typename Vec::value_type angle) {
			return from_normalized_axis_angle(vec::unsafe_normalize(axis), std::move(angle));
		}

		/// No normalization needed for unit quaternions.
		template <typename T> constexpr static unit_quaternion<T> unsafe_normalize(unit_quaternion<T> q) {
			return q;
		}
		/// Normalizes the given quaternion without checking if its magnitude is close to zero.
		template <typename T> inline constexpr static unit_quaternion<T> unsafe_normalize(quaternion<T> q) {
			T norm = q.magnitude();
			return unit_quaternion<T>(q.w() / norm, q.x() / norm, q.y() / norm, q.z() / norm);
		}
	};
}
