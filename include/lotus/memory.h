#pragma once

/// \file
/// Memory utilities.

namespace lotus::memory {
	/// Allocates memory.
	[[nodiscard]] inline void *allocate(std::size_t size) {
		return std::malloc(size);
	}
	/// Frees memory.
	inline void free(void *ptr) {
		std::free(ptr);
	}
}
