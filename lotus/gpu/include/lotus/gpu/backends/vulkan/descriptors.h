#pragma once

/// \file
/// Descriptor related classes.

#include <vulkan/vulkan.hpp>

#include "details.h"

namespace lotus::gpu::backends::vulkan {
	class command_list;
	class device;


	/// Contains a \p vk::UniqueDescriptorPool.
	class descriptor_pool {
		friend device;
	protected:
		/// Initializes this pool to empty.
		descriptor_pool(std::nullptr_t) {
		}
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

		/// Checks if this layout is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_layout.get();
		}
	private:
		vk::UniqueDescriptorSetLayout _layout; ///< The descriptor set layout.
		/// Index of the binding that has a varialbe number of descriptors.
		u32 _variable_binding_index = std::numeric_limits<u32>::max();
	};

	/// Contains a \p vk::UniqueDescriptorSet.
	class descriptor_set {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty object.
		descriptor_set(std::nullptr_t) {
		}

		/// Returns whether this descriptor set is valid.
		[[nodiscard]] bool is_valid() const {
			return static_cast<bool>(_set);
		}
	private:
		vk::UniqueDescriptorSet _set; ///< The descriptor set.
		/// Index of the binding that has a varialbe number of descriptors.
		u32 _variable_binding_index = std::numeric_limits<u32>::max();
	};
}
