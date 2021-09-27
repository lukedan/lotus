#pragma once

/// \file
/// An object similar to \p std::function but do not allocate additional memory.

#include <cstddef>
#include <cassert>
#include <array>

namespace lotus {
	constexpr std::size_t default_static_function_size = sizeof(void*[3]);
	/// A function type with a predictable memory footprint.
	template <typename FuncType, std::size_t StorageSize = default_static_function_size> struct static_function;
	/// Specialization for concrete function types.
	template <
		typename Ret, typename ...Args, std::size_t StorageSize
	> struct static_function<Ret(Args...), StorageSize> {
	public:
		/// Initializes \ref _storage with an empty function pointer.
		static_function(std::nullptr_t) {
			_set<Ret(*)(Args...)>(nullptr);
		}
		/// Initializes \ref _storage with the given callable object.
		template <typename Callable> static_function(Callable &&obj) {
			_set(std::forward<Callable>(obj));
		}
		/// No copy constructor.
		static_function(const static_function&) = delete;
		/// No copy assignment.
		static_function &operator=(const static_function&) = delete;
		/// Destroys the callable object.
		~static_function() {
			_reset();
		}

		/// Invokes the function.
		Ret operator()(Args &&...args) {
			return _get()->invoke(std::forward<Args>(args)...);
		}

		/// Tests if this function is valid.
		[[nodiscard]] bool is_empty() const {
			return !_get()->is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return !is_empty();
		}
	protected:
		/// Base class for callable objects.
		class _callable_base {
		public:
			/// Default virtual destructor.
			virtual ~_callable_base() = default;

			/// Invokes the callable object.
			virtual Ret invoke(Args &&...args) = 0;
			/// Returns if this callable object is valid.
			[[nodiscard]] virtual bool is_valid() const = 0;
		};
		/// A concrete callable object.
		template <typename Callable> class _callable : public _callable_base {
		public:
			/// Initializes \ref _callable_obj.
			explicit _callable(Callable obj) : _callable_obj(std::move(obj)) {
			}

			/// Invokes the callable object.
			Ret invoke(Args &&...args) override {
				return _callable_obj(std::forward<Args>(args)...);
			}
			/// Returns \p false only if this object contains an empty function pointer.
			[[nodiscard]] bool is_valid() const override {
				if constexpr (std::is_same_v<Callable, Ret(*)(Args...)>) {
					return _callable_obj != nullptr;
				}
				return true;
			}
		protected:
			[[no_unique_address]] Callable _callable_obj; ///< The callable object.
		};

		/// Creates a new \ref _callable in \ref _storage, assuming that any previous object living in it has been
		/// destroyed before the call or has not been initialized.
		template <typename Callable> void _set(Callable obj) {
			using callable_t = _callable<Callable>;
			static_assert(sizeof(callable_t) <= StorageSize, "Not enough capacity for static function");
			auto *ptr = new (_storage.data()) callable_t(std::move(obj));
			assert(static_cast<void*>(ptr) == _storage.data());
		}
		/// Frees any object in \ref _storage and leaves it empty.
		void _reset() {
			_get()->~_callable_base();
		}
		/// Returns the callable object.
		[[nodiscard]] _callable_base *_get() {
			return reinterpret_cast<_callable_base*>(_storage.data());
		}
		/// \overload
		[[nodiscard]] const _callable_base *_get() const {
			return reinterpret_cast<const _callable_base*>(_storage.data());
		}

		alignas(std::max_align_t) std::array<std::byte, StorageSize> _storage; ///< Storage for the callable object.
	};
}
