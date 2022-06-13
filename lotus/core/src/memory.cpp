#include "lotus/memory.h"

/// \file
/// Implementation of memory operations.

#include <mimalloc.h>
#include <mimalloc-new-delete.h>

namespace lotus::memory {
	namespace raw {
		void *allocate(size_alignment s) {
			return mi_aligned_alloc(s.alignment, s.size);
		}

		void free(void *ptr) {
			return mi_free(ptr);
		}
	}
}
