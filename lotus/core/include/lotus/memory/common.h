#pragma once

/// \file
/// Memory utilities.

#include <cstdlib>

#include "lotus/common.h"

namespace lotus::memory {
	/// Size and alignment information.
	struct size_alignment {
	public:
		/// Initializes all fields of this struct.
		constexpr size_alignment(usize sz, usize align) : size(sz), alignment(align) {
		}
		/// Returns the size and alignment of the given type.
		template <typename T> [[nodiscard]] constexpr inline static size_alignment of() {
			return size_alignment(sizeof(T), alignof(T));
		}
		/// Returns the size and alignment of an array of the given type and size.
		template <typename T> [[nodiscard]] constexpr inline static size_alignment of_array(usize count) {
			return size_alignment(sizeof(T) * count, alignof(T));
		}

		usize size; ///< Size.
		usize alignment; ///< Alignment.
	};


	namespace raw {
		/// Allocates memory.
		[[nodiscard]] std::byte *allocate(size_alignment);
		/// Frees memory.
		void free(std::byte*);

		/// Allocator using the basic raw allocation functions.
		class allocator {
		public:
			/// Allocates a block of memory with the given size and alignment.
			[[nodiscard]] std::byte *allocate(size_alignment s) const {
				return raw::allocate(s);
			}
			/// Frees the given memory block.
			void free(std::byte *ptr) const {
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

	/// Finds the smallest value larger than or equal to the input that satifies the alignment.
	[[nodiscard]] inline constexpr usize align_up(usize value, usize align) {
		return align * ((value + align - 1) / align);
	}
	/// Finds the largest value smaller than or equal to the input that satifies the alignment.
	[[nodiscard]] inline constexpr usize align_down(usize value, usize align) {
		return value - (value % align);
	}
	/// Checks that the given pointer is aligned.
	[[nodiscard]] inline bool is_aligned(std::byte *ptr, usize align) {
		return reinterpret_cast<std::uintptr_t>(ptr) % align == 0;
	}

	/// Poisons the given block of memory.
	void poison(std::byte *memory, usize size);
	/// Un-poisons the given block of memory.
	void unpoison(std::byte *memory, usize size);
}
