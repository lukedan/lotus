#pragma once

/// \file
/// Implementation details and helpers for the Metal backend.

#include <cstddef>
#include <utility>

#include "lotus/utils/static_function.h"
#include "lotus/gpu/common.h"

// forward declarations
namespace CA {
	class MetalDrawable;
}
namespace MTL {
	class CommandQueue;
	class Device;
}

namespace lotus::gpu::backends::metal::_details {
	// TODO
	/// Metal does not seem to have custom debug callbacks.
	using debug_message_id = int;
	/// Debug callback type for the Metal backend.
	using debug_message_callback =
		static_function<void(debug_message_severity, debug_message_id, std::u8string_view)>;

	/// Smart pointer class that automatically calls the \p retain() and \p release() methods.
	template <typename T> struct metal_ptr {
	public:
		/// Default constructor.
		metal_ptr() = default;
		/// Initializes this pointer to null.
		metal_ptr(std::nullptr_t) {
		}
		/// Move constructor.
		metal_ptr(metal_ptr &&src) noexcept : _ptr(std::exchange(src._ptr, nullptr)) {
		}
		/// Copy constructor.
		metal_ptr(const metal_ptr &src) : _ptr(src._ptr) {
			if (_ptr) {
				_ptr->retain();
			}
		}
		/// Move assignment.
		metal_ptr &operator=(metal_ptr &&src) noexcept {
			if (&src != this) {
				release();
				_ptr = std::exchange(src._ptr, nullptr);
			}
			return *this;
		}
		/// Copy assignment.
		metal_ptr &operator=(const metal_ptr &src) {
			if (&src != this) {
				release();
				_ptr = src._ptr;
				_ptr->retain();
			}
			return *this;
		}
		/// Releases the object, if any.
		~metal_ptr() {
			release();
		}

		/// Takes ownership of the given pointer. The input pointer should not be used afterwards.
		[[nodiscard]] static metal_ptr take_ownership(T *&&ptr) {
			return metal_ptr(std::exchange(ptr, nullptr));
		}
		/// Shares ownership of the given pointer.
		[[nodiscard]] static metal_ptr share_ownership(T *ptr) {
			ptr->retain();
			return metal_ptr(ptr);
		}

		/// Releases the object, if any.
		void release() {
			if (_ptr) {
				_ptr->release();
				_ptr = nullptr;
			}
		}

		/// Returns a pointer to the object.
		[[nodiscard]] T *get() const {
			return _ptr;
		}
		/// \overload
		[[nodiscard]] T *operator->() const {
			return _ptr;
		}
		/// \overload
		[[nodiscard]] T &operator*() const {
			return *_ptr;
		}

		/// Checks if this pointer points to a valid object.
		[[nodiscard]] bool is_valid() const {
			return _ptr != nullptr;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		T *_ptr = nullptr; ///< The pointer.

		/// Initializes \ref _ptr to the given value.
		explicit metal_ptr(T *p) : _ptr(p) {
		}
	};
	/// Convenience function for taking the ownership of a given Metal object.
	template <typename T> metal_ptr<T> take_ownership(T *&&p) {
		return metal_ptr<T>::take_ownership(std::move(p));
	}
	/// Convenience function for sharing the ownership of a given Metal object.
	template <typename T> metal_ptr<T> share_ownership(T *p) {
		return metal_ptr<T>::share_ownership(p);
	}
}
