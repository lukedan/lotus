#include "pbd/utils/stack_allocator.h"

/// \file
/// Implementation of the stack allocator.

namespace pbd {
	stack_allocator &stack_allocator::for_this_thread() {
		static thread_local stack_allocator _allocator;
		return _allocator;
	}
}
