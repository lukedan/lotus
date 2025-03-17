#pragma once

/// \file
/// Pool implementation.

#include <utility>
#include <span>

#include "lotus/common.h"
#include "lotus/containers/static_optional.h"

namespace lotus {
	template <typename Entry> class pool_manager;

	namespace _details {
		/// Used to determine the index type of a pool.
		template <typename Index, bool IsRaw> struct pool_index_type;
		/// For raw integral types, declare custom enum type.
		template <typename Index> struct pool_index_type<Index, true> {
			/// The index type.
			enum class type : Index {
				invalid = std::numeric_limits<Index>::max() ///< The invalid value.
			};
		};
		/// Otherwise, use the type directly. It must have an \p invalid static member.
		template <typename Index> struct pool_index_type<Index, false> {
			using type = Index;
		};
	}

	/// An entry in a pool. This entry is created without a valid object, and must be destroyed when the object has
	/// been freed.
	template <typename T, typename Index = u32> struct pool_entry {
		friend pool_manager<pool_entry>;
	public:
		using value_type = T; ///< Value type.
		/// Index type.
		using index_type =
			_details::pool_index_type<Index, std::is_integral_v<Index> && !std::is_enum_v<Index>>::type;

		/// Initializes this entry to empty.
		pool_entry(uninitialized_t) : _next(index_type::invalid), _allocated(false) {
		}
		/// Only empty entries can be moved.
		pool_entry(pool_entry &&src) : _next(src._next), _allocated(false) {
			crash_if(src._allocated.value_or(false));
		}
		/// No copy construction.
		pool_entry(const pool_entry&) = delete;
		/// Only empty entries can be moved.
		pool_entry &operator=(pool_entry &&src) {
			crash_if(src._allocated.value_or(false));
			crash_if(_allocated.value_or(false));
			_next = std::exchange(src._next, index_type::invalid);
		}
		/// No copy assignment.
		pool_entry &operator=(const pool_entry&) = delete;
		/// Checks that this entry has been properly freed.
		~pool_entry() {
			crash_if(_allocated.value_or(false));
			std::destroy_at(&_next);
		}

		/// Creates storage for a pool containing the given number of slots.
		template <typename Allocator> [[nodiscard]] inline static std::vector<pool_entry, Allocator> make_storage(
			usize capacity, const Allocator &allocator
		) {
			std::vector<pool_entry, Allocator> result(allocator);
			result.reserve(capacity);
			for (usize i = 0; i < capacity; ++i) {
				result.emplace_back(uninitialized);
			}
			return result;
		}
	private:
		union {
			T _object; ///< The object.
			index_type _next; ///< Index of the next element.
		};
		[[no_unique_address]] debug_value<bool> _allocated; ///< Whether this entry has been allocated.

		/// Creates and constructs an object in this entry. There must not be an object that has already been
		/// constructed.
		template <typename ...Args> T &_emplace(Args &&...args) {
			crash_if(_allocated.value_or(false));
			_allocated = true;
			std::destroy_at(&_next);
			std::construct_at(&_object, std::forward<Args>(args)...);
			return _object;
		}
		/// Destructs the object in this entry and puts the given index in its place. There must be an object that
		/// has been constructed.
		void _reset(index_type i = index_type::invalid) {
			crash_if(!_allocated.value_or(true));
			_allocated = false;
			std::destroy_at(&_object);
			std::construct_at(&_next, i);
		}
	};
	/// Manager class for a pool that can be used to allocate objects out of. The storage of this pool will be
	/// managed externally.
	template <typename Entry> class pool_manager {
	public:
		using entry_type = Entry; ///< Entry type.
		using value_type = Entry::value_type; ///< Value type.
		using index_type = entry_type::index_type; ///< Index type.

		/// Initializes this pool to empty.
		pool_manager(std::nullptr_t) {
		}
		/// Initializes this pool, and checks that all entries are free if possible.
		explicit pool_manager(std::span<entry_type> st) : _storage(st) {
			if constexpr (_has_markers) {
				for (usize i = 0; i < _storage.size(); ++i) {
					crash_if(_storage[i]._allocated.value);
				}
			}
		}
		/// Move constructor.
		pool_manager(pool_manager &&src) :
			_storage(std::exchange(src._storage, {})),
			_allocated(std::exchange(src._allocated, 0)),
			_head(std::exchange(src._head, index_type::invalid)) {
		}
		/// No copy construction.
		pool_manager(const pool_manager&) = delete;
		/// Move assignment.
		pool_manager &operator=(pool_manager &&src) {
			// need to free all entries before moving into this pool; otherwise they won't be freed
			crash_if(this != &src && _allocated > 0);
			_storage   = std::exchange(src._storage, {});
			_allocated = std::exchange(src._allocated, 0);
			_head      = std::exchange(src._head, index_type::invalid);
			return *this;
		}
		/// No copy assignment.
		pool_manager &operator=(const pool_manager&) = delete;
		/// Checks that all entries have been freed.
		~pool_manager() {
			crash_if(_allocated > 0);
		}

		/// Allocates a new entry from the pool, and initializes it with the given arguments.
		template <typename ...Args> std::pair<index_type, value_type&> allocate(Args &&...args) {
			crash_if(is_full());
			index_type slot;
			if (_head == index_type::invalid) {
				slot = static_cast<index_type>(_allocated);
			} else {
				slot = _head;
				_head = _storage[std::to_underlying(_head)]._next;
			}
			++_allocated;
			value_type &obj = _storage[std::to_underlying(slot)]._emplace(std::forward<Args>(args)...);
			return { slot, obj };
		}
		/// Frees the given entry.
		void free(index_type i) {
			_storage[std::to_underlying(i)]._reset(_head);
			_head = i;
			--_allocated;
		}

		/// Returns the object at the given index.
		value_type &at(index_type i) {
			crash_if(!_storage[std::to_underlying(i)]._allocated.value_or(true));
			return _storage[std::to_underlying(i)]._object;
		}
		/// \overload
		const value_type &at(index_type i) const {
			crash_if(!_storage[std::to_underlying(i)]._allocated.value_or(true));
			return _storage[std::to_underlying(i)]._object;
		}
		/// Returns the object at the given index.
		value_type &operator[](index_type i) {
			return at(i);
		}
		/// \overload
		const value_type &operator[](index_type i) const {
			return at(i);
		}

		/// Returns the capacity of this pool.
		[[nodiscard]] constexpr usize get_capacity() const {
			return _storage.size();
		}
		/// Returns whether this pool is full, i.e., no more entries can be allocated from the pool.
		[[nodiscard]] constexpr bool is_full() const {
			return _allocated == get_capacity();
		}
	private:
		/// Whether there are markers indicating if an entry contains a valid object.
		constexpr static bool _has_markers = decltype(entry_type::_allocated)::is_enabled;

		std::span<entry_type> _storage; ///< The storage of this pool.
		usize _allocated = 0; ///< Number of entries that have been allocated.
		index_type _head = index_type::invalid; ///< The first element in the free list.
	};
}
