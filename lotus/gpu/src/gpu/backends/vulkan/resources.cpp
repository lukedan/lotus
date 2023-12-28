#include "lotus/gpu/backends/vulkan/resources.h"

/// \file
/// Implementation of Vulkan resources.

#include "lotus/gpu/backends/vulkan/details.h"

namespace lotus::gpu::backends::vulkan {
	namespace _details {
		std::byte *memory_block::map() {
			if (_num_maps == 0) {
				crash_if(_mapped_addr);
				_mapped_addr = static_cast<std::byte*>(unwrap(
					_memory.getOwner().mapMemory(_memory.get(), 0, VK_WHOLE_SIZE)
				));
			}
			++_num_maps;
			return _mapped_addr;
		}

		void memory_block::unmap() {
			crash_if(_num_maps == 0);
			--_num_maps;
			if (_num_maps == 0) {
				_memory.getOwner().unmapMemory(_memory.get());
				_mapped_addr = nullptr;
			}
		}
	}
}
