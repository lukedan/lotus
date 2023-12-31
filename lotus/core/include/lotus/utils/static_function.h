#pragma once

/// \file
/// An object similar to \p std::function but do not allocate additional memory.

#include <cstddef>
#include <array>
#include <functional>

#include "lotus/memory/common.h"

namespace lotus {
	/// Default storage size for static functions.
	constexpr std::size_t default_static_function_size = sizeof(void*[2]);
	/// A function type with a predictable memory footprint.
	template <typename FuncType, std::size_t StorageSize = default_static_function_size> struct static_function;
	/// Specialization for concrete function types.
	template <
		typename Ret, typename ...Args, std::size_t StorageSize
	> struct static_function<Ret(Args...), StorageSize> {
	public:
		/// Whether to poison the function object storage when it's invalid.
		constexpr static bool should_poison_storage = is_debugging;

		/// Initializes \ref _storage with an empty function pointer.
		static_function(std::nullptr_t) {
			_maybe_poison_storage();
		}
		/// Initializes \ref _storage with the given callable object.
		template <typename Callable> static_function(Callable &&obj) {
			_set(std::forward<Callable>(obj));
		}
		/// Move construction from another function object.
		template <std::size_t OtherSize> static_function(static_function<Ret(Args...), OtherSize> &&src) {
			_move_from(std::move(src));
		}
		/// No copy constructor.
		static_function(const static_function&) = delete;
		/// Move assignment from another function object.
		template <std::size_t OtherSize> static_function &operator=(static_function<Ret(Args...), OtherSize> &&src) {
			if (&src != this) {
				_reset();
				_move_from(std::move(src));
			}
			return *this;
		}
		/// Resets this function object.
		static_function &operator=(std::nullptr_t) {
			_reset();
			return *this;
		}
		/// Assigns a function object to this function.
		template <typename Callable> static_function &operator=(Callable &&obj) {
			_reset();
			_set(std::forward<Callable>(obj));
			return *this;
		}
		/// No copy assignment.
		static_function &operator=(const static_function&) = delete;
		/// Destroys the callable object.
		~static_function() {
			_reset();
		}

		/// Invokes the function.
		Ret operator()(Args ...args) {
			return _impl.invoke(_storage.data(), std::forward<Args>(args)...);
		}

		/// Tests if this function is valid.
		[[nodiscard]] bool is_empty() const {
			return !_impl.is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return !is_empty();
		}
	protected:
		/// Function pointers for all operations.
		struct _impl_t {
			/// Initializes all function pointers to \p nullptr.
			_impl_t(std::nullptr_t) {
			}

			/// Both function pointers are required to be non-null. Here we only check one.
			[[nodiscard]] bool is_valid() const {
				return invoke != nullptr;
			}

			Ret (*invoke)(void*, Args&&...) = nullptr; ///< Invokes the function object.
			/// Moves the function object from one place to another, and calls the destructor of the old object. If
			/// \p to is \p nullptr, the object is simply destroyed. If there's not enough space in the destination
			/// buffer, this function does nothing (not even destroying the source object) and simply returns
			/// \p false.
			bool (*move)(void *from, void *to, std::size_t to_size) = nullptr;
		};

		/// Creates a new \ref _callable in \ref _storage, assuming this object does not currently contain a valid
		/// function object.
		template <typename Callable> void _set(Callable &&obj) {
			using callable_t = std::decay_t<Callable>;

			crash_if(_impl.is_valid());
			static_assert(sizeof(callable_t) <= StorageSize, "Not enough capacity for static function");
			new (_storage.data()) callable_t(std::move(obj));
			_impl.invoke = [](void *p, Args &&...args) -> Ret {
				return (*static_cast<callable_t*>(p))(std::forward<Args>(args)...);
			};
			_impl.move = [](void *from, void *to, std::size_t to_size) -> bool {
				auto &from_obj = *static_cast<callable_t*>(from);
				if (to) {
					if (to_size < sizeof(callable_t)) {
						return false;
					}
					new (to) callable_t(std::move(from_obj));
				}
				from_obj.~callable_t();
				return true;
			};
		}
		/// Moves the callable from the given object to this object, assuming that this object does not currently
		/// contain a valid function object.
		template <std::size_t OtherSize> void _move_from(static_function<Ret(Args...), OtherSize> &&src) {
			crash_if(_impl.is_valid());
			if (src._impl.is_valid()) {
				crash_if(!src._impl.move(src._storage.data(), _storage.data(), _storage.size()));
				_impl = std::exchange(src._impl, nullptr);
				src._maybe_poison_storage();
			} else {
				_impl = nullptr;
			}
		}
		/// Frees any object in \ref _storage and leaves it empty.
		void _reset() {
			if (_impl.is_valid()) {
				_impl.move(_storage.data(), nullptr, 0);
				_impl = nullptr;
				_maybe_poison_storage();
			}
		}

		/// Poisons the storage if required.
		void _maybe_poison_storage() {
			if constexpr (should_poison_storage) {
				memory::poison(_storage.data(), _storage.size());
			}
		}

		alignas(std::max_align_t) std::array<std::byte, StorageSize> _storage; ///< Storage for the callable object.
		[[no_unique_address]] _impl_t _impl = nullptr; ///< Used to actually invoke and move the function.
	};
}
