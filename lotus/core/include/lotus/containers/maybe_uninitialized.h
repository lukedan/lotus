#pragma once

/// \file
/// A structure that holds an object that may or may not be initialized.

#include <cstddef>

#include "lotus/common.h"
#include "lotus/memory/common.h"
#include "static_optional.h"

namespace lotus {
	/// Holds an object that may or may not be initialized. Holds additional data for debugging when in debug mode.
	template <typename T, bool UseDebugMarker = is_debugging> struct maybe_uninitialized {
	public:
		/// The object is not initialized.
		maybe_uninitialized(uninitialized_t) : _is_initialized(false) {
			_maybe_poison_storage();
		}
		/// No copy & move construction.
		maybe_uninitialized(const maybe_uninitialized&) = delete;
		/// No copy & move assignment.
		maybe_uninitialized &operator=(const maybe_uninitialized&) = delete;
		/// Checks that the object has been properly freed.
		~maybe_uninitialized() {
			crash_if(_is_initialized.value_or(false));
		}

		/// Initializes the value.
		template <typename ...Args> void initialize(Args &&...args) {
			if constexpr (_is_initialized.is_enabled) {
				crash_if(_is_initialized.value_or(false));
				_is_initialized.value = true;
			}
			_maybe_unpoison_storage();
			new (&_value) T(std::forward<Args>(args)...);
		}
		/// Disposes of the value.
		void dispose() {
			if constexpr (_is_initialized.is_enabled) {
				crash_if(!_is_initialized.value_or(true));
				_is_initialized.value = false;
			}
			_value.~T();
			_maybe_poison_storage();
		}

		/// Returns the object.
		[[nodiscard]] T &get() {
			crash_if(!_is_initialized.value_or(true));
			return _value;
		}
		/// \overload
		[[nodiscard]] const T &get() const {
			crash_if(!_is_initialized.value_or(true));
			return _value;
		}
		/// \overload
		[[nodiscard]] T &operator*() {
			return get();
		}
		/// \overload
		[[nodiscard]] const T &operator*() const {
			return get();
		}
		/// \overload
		[[nodiscard]] T *operator->() {
			return &get();
		}
		/// \overload
		[[nodiscard]] const T *operator->() const {
			return &get();
		}
	private:
		union {
			T _value; ///< Value.
		};
		/// Whether \ref _value is a valid object.
		[[no_unique_address]] static_optional<bool, UseDebugMarker> _is_initialized;

		/// Poisons this object if debugging.
		void _maybe_poison_storage() {
			if constexpr (is_debugging) {
				memory::poison(&_value, sizeof(_value));
			}
		}
		/// Unpoisons this object if debugging.
		void _maybe_unpoison_storage() {
			if constexpr (is_debugging) {
				memory::unpoison(&_value, sizeof(_value));
			}
		}
	};
}
