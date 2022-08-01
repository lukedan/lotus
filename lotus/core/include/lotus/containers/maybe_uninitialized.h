#pragma once

/// \file
/// A structure that holds an object that may or may not be initialized.

#include <cstddef>

#include "lotus/common.h"
#include "lotus/memory.h"

namespace lotus {
	/// Holds an object that may or may not be initialized. Holds additional data for debugging when in debug mode.
	template <typename T> struct maybe_uninitialized {
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
			if constexpr (_is_initialized.is_enabled) {
				assert(!_is_initialized.value);
			}
		}

		/// Initializes the value.
		template <typename ...Args> void initialize(Args &&...args) {
			if constexpr (_is_initialized.is_enabled) {
				assert(!_is_initialized.value);
				_is_initialized.value = true;
			}
			new (&_value) T(std::forward<Args>(args)...);
		}
		/// Disposes of the value.
		void dispose() {
			if constexpr (_is_initialized.is_enabled) {
				assert(_is_initialized.value);
				_is_initialized.value = false;
			}
			_value.~T();
			_maybe_poison_storage();
		}

		/// Returns the object.
		[[nodiscard]] T &get() {
			if constexpr (_is_initialized.is_enabled) {
				assert(_is_initialized.value);
			}
			return _value;
		}
		/// \overload
		[[nodiscard]] const T &get() const {
			if constexpr (_is_initialized.is_enabled) {
				assert(_is_initialized.value);
			}
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
		[[no_unique_address]] debug_value<bool> _is_initialized; ///< Whether \ref _value is a valid object.

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
