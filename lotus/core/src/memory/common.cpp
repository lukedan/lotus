#include "lotus/memory/common.h"

/// \file
/// Implementation of memory operations.

#ifdef _MSC_VER
#	undef __SANITIZE_ADDRESS__ // HACK: msvc doesn't have the include path set up properly
#endif

#ifdef LOTUS_USE_MIMALLOC
#	include <mimalloc.h>
#	include <mimalloc-new-delete.h>
#endif
#ifdef __SANITIZE_ADDRESS__
#	include <sanitizer/asan_interface.h>
#endif

namespace lotus::memory {
	namespace raw {
		std::byte *allocate(size_alignment s) {
			// we may need to massage the size and alignment to satisfy the allocator's requirements
			[[maybe_unused]] const usize align = std::max(s.alignment, sizeof(void*));
			[[maybe_unused]] const usize aligned_size = memory::align_up(s.size, align);
			return static_cast<std::byte*>(
#ifdef LOTUS_USE_MIMALLOC
				mi_aligned_alloc(s.alignment, s.size)
#else
#	ifdef _MSC_VER
				_aligned_malloc(s.size, s.alignment)
#	else
				std::aligned_alloc(align, aligned_size)
#	endif
#endif
			);
		}

		void free(std::byte *ptr) {
#ifdef LOTUS_USE_MIMALLOC
			mi_free(ptr);
#else
#	ifdef _MSC_VER
			_aligned_free(ptr);
#	else
			std::free(ptr);
#	endif
#endif
		}
	}

	void poison(std::byte *memory, usize size) {
#ifdef __SANITIZE_ADDRESS__
		__asan_poison_memory_region(memory, size);
#else
		std::memset(memory, 0xCD, size);
#endif
	}
	void unpoison([[maybe_unused]] std::byte *memory, [[maybe_unused]] usize size) {
#ifdef __SANITIZE_ADDRESS__
		__asan_unpoison_memory_region(memory, size);
#endif
	}
}
