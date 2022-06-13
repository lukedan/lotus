#pragma once

/// \file
/// Pool implementation.

#include <deque>

#include "lotus/common.h"
#include "lotus/utils/index.h"
#include "static_optional.h"

namespace lotus {
	/// Index types used with pools.
	template <typename TypeTag> struct pool_index {
		/// Tag type for indices used with pools.
		struct tag_type {
		};

		/// Index type.
		template <typename Index = std::uint32_t> using index_t = typed_index<tag_type, Index>;
		/// Optional index type.
		template <typename Index = std::uint32_t> using optional_index_t = optional_typed_index<tag_type, Index>;
	};
	// TODO allocator
	/// A pool of objects that supports fast allocation and deallocation.
	template <typename T, typename TypeTag = T, typename Index = std::uint32_t> class pool {
	public:
		/// Index type.
		using index_type = typename pool_index<TypeTag>::template index_t<Index>;
		/// Optional index type.
		using optional_index_type = typename pool_index<TypeTag>::template optional_index_t<Index>;

		/// No default constructor.
		pool() = delete;
		/// Creates a pool with the given size.
		[[nodiscard]] inline static pool create(Index size) {
			return pool(size);
		}

		/// Allocates an entry from this pool. Asserts if the pool is empty.
		template <typename ...Args> [[nodiscard]] index_type allocate(Args &&...args) {
			assert(_head);
			index_type result = _head.get();
			_head = _pool[result.get_value()].allocate(std::forward<Args>(args)...);
			return result;
		}
		/// Frees the given element.
		void free(index_type i) {
			_pool[i.get_value()].free(_head);
			_head = i;
		}

		/// Returns the element at the given index.
		[[nodiscard]] T &at(index_type i) {
			auto &res = _pool[i.get_value()];
			if constexpr (decltype(res.allocated)::is_enabled) {
				assert(res.allocated.value);
			}
			return res.object;
		}
		/// Returns the element at the given index.
		[[nodiscard]] const T &at(index_type i) const {
			auto &res = _pool[i.get_value()];
			if constexpr (decltype(res.allocated)::is_enabled) {
				assert(res.allocated.value);
			}
			return res.object;
		}

		/// Returns the capacity of this pool.
		[[nodiscard]] Index get_capacity() const {
			return static_cast<Index>(_pool.size());
		}
	protected:
		/// An entry in the pool.
		struct _entry {
			union {
				T object; ///< The object.
				optional_index_type next; ///< Index of the next element.
			};
			[[no_unique_address]] debug_value<bool> allocated; ///< Whether this entry has been allocated.

			/// Initializes this entry to empty.
			_entry(std::nullptr_t) : allocated(false) {
			}
			/// Dummy copy constructor.
			_entry(const _entry &src) : allocated(false) {
				if constexpr (src.allocated.is_enabled) {
					assert(!src.allocated.value);
				}
			}
			/// Checks that this entry has been properly freed.
			~_entry() {
				if constexpr (allocated.is_enabled) {
					assert(!allocated.value);
				}
				next.~optional_index_type();
			}

			/// Allocates this entry and returns a index to the next element.
			template <typename ...Args> [[nodiscard]] optional_index_type allocate(Args &&...args) {
				if constexpr (allocated.is_enabled) {
					assert(!allocated.value);
					allocated.value = true;
				}
				optional_index_type n = next;
				next.~optional_index_type();
				new (&object) T(std::forward<Args>(args)...);
				return n;
			}
			/// Frees this entry and sets \ref next.
			void free(optional_index_type n) {
				if constexpr (allocated) {
					assert(allocated.value);
					allocated.value = false;
				}
				object.~T();
				new (&next) optional_index_type(n);
			}
		};

		/// Initializes this pool with the given size.
		explicit pool(Index size) : _head(index_type(0)) {
			_pool.reserve(size);
			for (Index i = 0; i < size; ++i) {
				auto &entry = _pool.emplace_back(nullptr);
				if constexpr (decltype(entry.allocated)::is_enabled) {
					entry.allocated.value = false;
				}
				new (&entry.next) optional_index_type(i + 1 < size ? optional_index_type(i + 1) : nullptr);
			}
		}

		std::vector<_entry> _pool; ///< The objects.
		optional_index_type _head; ///< Index of the first entry that has not been allocated.
	};
}
