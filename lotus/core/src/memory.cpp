#include "lotus/memory.h"

/// \file
/// Implementation of memory operations.

#ifdef LOTUS_USE_MIMALLOC
#	include <mimalloc.h>
#	include <mimalloc-new-delete.h>
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
}
