#pragma once

/// \file
/// A compile-time optional type.

#include "lotus/common.h"

namespace lotus {
	/// A compile-time optional data member. Should be used with \p [[no_unique_address]].
	template <typename T, bool Enable> struct static_optional {
		static_assert(Enable, "Specialization should have been selected");
		using value_type = T; ///< Value type.
		constexpr static bool is_enabled = true; ///< Whether this type is enabled.
		/// Returns whether this value is enabled.
		[[nodiscard]] constexpr explicit operator bool() const {
			return is_enabled;
		}

		/// Default constructor.
		static_optional() = default;
		/// Initializes the underlying value with a single parameter.
		template <
			typename Arg, std::enable_if_t<!std::is_same_v<std::decay_t<Arg>, static_optional>, int> = 0
		> constexpr static_optional(Arg &&arg) : value(std::forward<Arg>(arg)) {
		}
		/// Initializes the underlying value with multiple parameters.
		template <typename Arg1, typename Arg2, typename ...Args> constexpr static_optional(
			Arg1 &&arg1, Arg2 &&arg2, Args &&...args
		) : value(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2), std::forward<Args>(args)...) {
		}
		/// Default move constructor.
		static_optional(static_optional&&) = default;
		/// Default copy constructor.
		static_optional(const static_optional&) = default;
		/// Default move assignment.
		static_optional &operator=(static_optional&&) = default;
		/// Default copy assignment.
		static_optional &operator=(const static_optional&) = default;

		/// Returns the value.
		[[nodiscard]] T *operator->() {
			return &value;
		}
		/// \overload
		[[nodiscard]] const T *operator->() const {
			return &value;
		}
		/// Returns the value.
		[[nodiscard]] T &operator*() {
			return value;
		}
		/// \overload
		[[nodiscard]] const T &operator*() const {
			return value;
		}

		/// Returns \ref value.
		constexpr const T &value_or(const T&) const {
			return value;
		}
		/// Calls the callback with the stored value.
		template <typename Cb> void if_enabled(Cb &&callback) {
			callback(value);
		}

		T value; ///< The value.
	};
	/// Specialization for disabled \ref static_optional objects.
	template <typename T> struct static_optional<T, false> {
		using value_type = T; ///< Value type.
		constexpr static bool is_enabled = false; ///< Whether this type is enabled.
		/// Returns whether this value is enabled.
		[[nodiscard]] constexpr explicit operator bool() const {
			return is_enabled;
		}

		/// Dummy constructor.
		template <typename ...Args> constexpr static_optional(Args&&...) {
		}

		/// Returns the given value.
		constexpr T value_or(T value) const {
			return value;
		}
		/// Does nothing.
		template <typename Cb> void if_enabled(Cb&&) {
		}
	};

	/// Type for values used only when debugging.
	template <typename T> using debug_value = static_optional<T, is_debugging>;
}
