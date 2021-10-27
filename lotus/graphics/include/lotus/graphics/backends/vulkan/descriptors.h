#pragma once

/// \file
/// Descriptor related classes.

#include <vulkan/vulkan.hpp>

#include "details.h"

namespace lotus::graphics::backends::vulkan {
	class command_list;
	class device;


	/// Contains a \p vk::UniqueDescriptorPool.
	class descriptor_pool {
		friend device;
	protected:
	private:
		vk::UniqueDescriptorPool _pool; ///< The descriptor pool.
	};

	/// Contains a \p vk::UniqueDescriptorSetLayout.
	class descriptor_set_layout {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty object.
		descriptor_set_layout(std::nullptr_t) {
		}
	private:
		vk::UniqueDescriptorSetLayout _layout; ///< The descriptor set layout.
	};

	/// Contains a \p vk::UniqueDescriptorSet.
	class descriptor_set {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty object.
		descriptor_set(std::nullptr_t) {
		}
	private:
		vk::UniqueDescriptorSet _set; ///< The descriptor set.
	};
}
