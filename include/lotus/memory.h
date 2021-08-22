#pragma once

/// \file
/// Memory utilities.

#include <cstdlib>

namespace lotus::memory {
	namespace raw {
		/// Allocates memory.
		[[nodiscard]] inline void *allocate(std::size_t size, std::size_t align) {
#ifdef _MSC_VER
			return _aligned_malloc(size, align);
#else
			return std::aligned_alloc(size, align);
#endif
		}
		/// Frees memory.
		inline void free(void *ptr) {
#ifdef _MSC_VER
			_aligned_free(ptr);
#else
			std::free(ptr);
#endif
		}
	}


	/// A category marker for an allocation.
	enum class allocation_category : std::uint8_t {
		graphics, ///< Allocation for graphics.
		physics, ///< Allocation for physics.

		num_heaps ///< The total number of memory heaps.
	};
}
