#pragma once

/// \file
/// Quaternions.

#include <cassert>
#include <type_traits>

#include "vector.h"

namespace pbd {
	/// Whether this quaternion is a unit quaternion.
	enum class quaternion_kind {
		arbitrary, ///< Quaternion with arbitrary magnitude.
		unit ///< Unit quaternion with a magnitude of 1.
	};

	template <typename T> struct quat;

	/// A quaternion.
	template <typename T, quaternion_kind Kind = quaternion_kind::arbitrary> struct quaternion {
		friend quat<T>;
	public:
		using value_type = T; ///< Value type.
		constexpr static quaternion_kind kind = Kind; ///< Whether this quaternion is a unit quaternion.


		/// Default move constructor.
		constexpr quaternion(quaternion&&) = default;
		/// Default copy constructor.
		constexpr quaternion(const quaternion&) = default;
		/// Default move assignment.
		constexpr quaternion &operator=(quaternion&&) = default;
		/// Default copy assignment.
		constexpr quaternion &operator=(const quaternion&) = default;


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
		> friend constexpr quaternion operator+(quaternion<T, Kind> lhs, const quaternion<T, OtherKind> &rhs) {
			lhs += rhs;
			return std::move(lhs);
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
		> friend constexpr quaternion operator-(quaternion<T, Kind> lhs, const quaternion<T, OtherKind> &rhs) {
			lhs -= rhs;
			return std::move(lhs);
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
		template <
			typename Dummy = int, std::enable_if_t<Kind == quaternion_kind::arbitrary, Dummy> = 0
		> friend constexpr quaternion operator/(quaternion<T, Kind> lhs, const T &rhs) {
			lhs /= rhs;
			return std::move(lhs);
		}


		/// Returns the squared magnitude of this quaternion.
		[[nodiscard]] constexpr T squared_magnitude() const {
			return _x * _x + _y * _y + _z * _z + _w * _w;
		}
		/// Returns the square root of \ref squared_magnitude().
		[[nodiscard]] constexpr T magnitude() const {
			return std::sqrt(squared_magnitude());
		}

		/// Returns the rotation axis. This is unnormalized even for unit quaternions.
		[[nodiscard]] constexpr cvec3_t<T> axis() const {
			return cvec3<T>::create({ x(), y(), z() });
		}

		[[nodiscard]] constexpr quaternion inverse() const {
			quaternion result(_w, -_x, -_y, -_z);
			if constexpr (Kind != quaternion_kind::unit) {
				result /= result.squared_magnitude();
			}
			return result;
		}
	protected:
		T
			_w, ///< The cosine of half the rotation angle.
			_x, ///< Rotation axis X times the sine of half the rotation angle.
			_y, ///< Rotation axis Y times the sine of half the rotation angle.
			_z; ///< Rotation axis Z times the sine of half the rotation angle.

		/// Default constructor. Does not initialize the components.
		quaternion() = default;
		/// Initializes all components of this quaternion.
		constexpr quaternion(T cw, T cx, T cy, T cz) :
			_w(std::move(cw)), _x(std::move(cx)), _y(std::move(cy)), _z(std::move(cz)) {
		}
	};
	template <typename T> using unit_quaternion = quaternion<T, quaternion_kind::unit>; ///< Unit quaternions.


	/// Quaternion utilities.
	template <typename T> struct quat {
		using quaternion_t = quaternion<T>; ///< Arbitrary quaternions.
		using unit_quaternion_t = unit_quaternion<T>; ///< Unit quaternions.

		/// Returns an uninitialized quaternion.
		[[nodiscard]] inline static quaternion_t uninitialized() {
			return quaternion_t();
		}
		/// Creates a quaternion using the given elements.
		[[nodiscard]] inline static constexpr quaternion_t from_wxyz(std::initializer_list<T> elems) {
			assert(elems.size() == 4);
			auto it = elems.begin();
			T w = std::move(*it);
			T x = std::move(*++it);
			T y = std::move(*++it);
			T z = std::move(*++it);
			return quaternion_t(std::move(w), std::move(x), std::move(y), std::move(z));
		}
		/// Returns the `zero' quaternion (1, 0, 0, 0).
		[[nodiscard]] inline static constexpr unit_quaternion_t zero() {
			return unit_quaternion_t(static_cast<T>(1), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0));
		}
		/// Creates a quaternion from the given normalized axis and rotation angle.
		template <typename Vec> [[nodiscard]] inline static constexpr std::enable_if_t<
			Vec::dimensionality == 3, unit_quaternion_t
		> from_normalized_axis_angle(const Vec &axis, T angle) {
			angle /= static_cast<T>(2);
			T w = std::cos(angle);
			T sin_half = std::sin(angle);
			return unit_quaternion_t(w, sin_half * axis[0], sin_half * axis[1], sin_half * axis[2]);
		}
		/// Creates a quaternion from the given axis and rotation angle. This function calls
		/// \ref vec::unsafe_normalize() to normalize the rotation axis; use \ref 
		template <typename Vec> [[nodiscard]] inline static constexpr std::enable_if_t<
			Vec::dimensionality == 3, unit_quaternion_t
		> from_axis_angle(const Vec &axis, T angle) {
			return from_normalized_axis_angle(vec::unsafe_normalize(axis), std::move(angle));
		}
	};

	using quatf = quat<float>; ///< Utilities for quaternions of \p float.
	using quatd = quat<double>; ///< Utilities for quaternions of \p double.
}
