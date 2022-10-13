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
		void *allocate(size_alignment s) {
#ifdef LOTUS_USE_MIMALLOC
			return mi_aligned_alloc(s.alignment, s.size);
#else
#	ifdef _MSC_VER
			return _aligned_malloc(s.size, s.alignment);
#	endif
#endif
		}

		void free(void *ptr) {
#ifdef LOTUS_USE_MIMALLOC
			mi_free(ptr);
#else
#	ifdef _MSC_VER
			_aligned_free(ptr);
#	endif
#endif
		}
	}

	void poison(void *memory, std::size_t size) {
#ifdef __SANITIZE_ADDRESS__
		__asan_poison_memory_region(memory, size);
#else
		std::memset(memory, 0xCD, size);
#endif
	}
	void unpoison([[maybe_unused]] void *memory, [[maybe_unused]] std::size_t size) {
#ifdef __SANITIZE_ADDRESS__
		__asan_unpoison_memory_region(memory, size);
#endif
	}
}
