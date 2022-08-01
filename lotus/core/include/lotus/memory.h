#pragma once

/// \file
/// Memory utilities.

#include <cstdlib>

#include "common.h"

namespace lotus::memory {
	/// Size and alignment information.
	struct size_alignment {
	public:
		/// Initializes all fields of this struct.
		constexpr size_alignment(std::size_t sz, std::size_t align) : size(sz), alignment(align) {
		}
		/// Returns the size and alignment of the given type.
		template <typename T> [[nodiscard]] constexpr inline static size_alignment of() {
			return size_alignment(sizeof(T), alignof(T));
		}
		/// Returns the size and alignment of an array of the given type and size.
		template <typename T> [[nodiscard]] constexpr inline static size_alignment of_array(std::size_t count) {
			return size_alignment(sizeof(T) * count, alignof(T));
		}

		std::size_t size; ///< Size.
		std::size_t alignment; ///< Alignment.
	};


	namespace raw {
		/// Allocates memory.
		[[nodiscard]] void *allocate(size_alignment);
		/// Frees memory.
		void free(void*);

		/// Allocator using the basic raw allocation functions.
		class allocator {
		public:
			/// Allocates a block of memory with the given size and alignment.
			[[nodiscard]] void *allocate(size_alignment s) const {
				return raw::allocate(s);
			}
			/// Frees the given memory block.
			void free(void *ptr) const {
				raw::free(ptr);
			}
		};
	}


	/// A category marker for an allocation.
	enum class allocation_category {
		graphics, ///< Allocation for graphics.
		physics, ///< Allocation for physics.

		num_heaps ///< The total number of memory heaps.
	};

	/// Aligns the given size.
	[[nodiscard]] inline constexpr std::size_t align_size(std::size_t size, std::size_t align) {
		return align * ((size + align - 1) / align);
	}
	/// Checks that the given pointer is aligned.
	[[nodiscard]] inline bool is_aligned(void *ptr, std::size_t align) {
		return reinterpret_cast<std::uintptr_t>(ptr) % align == 0;
	}

	/// Poisons the given block of memory.
	void poison(void *memory, std::size_t size);
	/// Un-poisons the given block of memory.
	void unpoison(void *memory, std::size_t size);


	/// A RAII memory block.
	template <typename Allocator = raw::allocator> struct block {
	public:
		/// Creates an empty block.
		block(std::nullptr_t, Allocator alloc = Allocator()) : _allocator(std::move(alloc)), _ptr(nullptr) {
		}
		/// Passes the given pointer to be managed by a \ref block.
		[[nodiscard]] inline static block manage(void *ptr, Allocator alloc = Allocator()) {
			return block(ptr, std::move(alloc));
		}
		/// Allocates a new block.
		[[nodiscard]] inline static block allocate(size_alignment s, Allocator alloc = Allocator()) {
			void *ptr = alloc.allocate(s);
			return block::manage(ptr, std::move(alloc));
		}
		/// Move constructor.
		block(block &&src) : _allocator(std::move(src._allocator)), _ptr(std::exchange(src._ptr, nullptr)) {
		}
		/// No copy construction.
		block(const block&) = delete;
		/// Move assignment.
		block &operator=(block &&src) {
			if (&src != this) {
				if (_ptr) {
					_allocator.free(_ptr);
				}
			}
			_ptr = std::exchange(src._ptr, nullptr);
			return *this;
		}
		/// No copy assignment.
		block &operator=(const block&) = delete;
		/// Frees the memory block if necessary.
		~block() {
			reset();
		}

		/// Returns the pointer to the block.
		[[nodiscard]] void *get() const {
			return _ptr;
		}

		/// Frees the memory block if necessary.
		void reset() {
			if (_ptr) {
				_allocator.free(_ptr);
				_ptr = nullptr;
			}
		}

		/// Returns whether this object refers to a block of memory.
		[[nodiscard]] bool is_valid() const {
			return _ptr;
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		[[no_unique_address]] Allocator _allocator; ///< Allocator.
		[[no_unique_address]] void *_ptr; ///< Pointer to the memory block.

		/// Initializes all fields of this struct.
		block(void *p, Allocator alloc) : _allocator(std::move(alloc)), _ptr(p) {
		}
	};
}
