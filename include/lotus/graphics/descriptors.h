#pragma once

/// \file
/// Descriptor pools.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class device;


	/// A pool of descriptors that descriptor sets can be allocated from.
	class descriptor_pool : public backend::descriptor_pool {
		friend device;
	public:
		/// No copy construction.
		descriptor_pool(const descriptor_pool&) = delete;
		/// No copy assignment.
		descriptor_pool &operator=(const descriptor_pool&) = delete;
	protected:
		/// Initializes the base type.
		descriptor_pool(backend::descriptor_pool base) : backend::descriptor_pool(std::move(base)) {
		}
	};
}
