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

	/// Specifies the layout of a descriptor set.
	class descriptor_set_layout : public backend::descriptor_set_layout {
		friend device;
	public:
		/// No copy construction.
		descriptor_set_layout(const descriptor_set_layout&) = delete;
		/// No copy assignment.
		descriptor_set_layout &operator=(const descriptor_set_layout&) = delete;
	protected:
		/// Initializes the base type.
		descriptor_set_layout(backend::descriptor_set_layout base) :
			backend::descriptor_set_layout(std::move(base)) {
		}
	};
}
