#pragma once

/// \file
/// RAII memory blocks.

namespace lotus::memory {
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
				_allocator = std::exchange(src._allocator, Allocator());
				_ptr = std::exchange(src._ptr, nullptr);
			}
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
