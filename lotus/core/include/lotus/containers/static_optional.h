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

		/// Constructor.
		template <typename ...Args> constexpr static_optional(Args &&...args) : value(std::forward<Args>(args)...) {
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
		[[nodiscard]] T *operator->() const {
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

		/// Does nothing.
		template <typename Cb> void if_enabled(Cb&&) {
		}
	};

	/// Type for values used only when debugging.
	template <typename T> using debug_value = static_optional<T, is_debugging>;
}
