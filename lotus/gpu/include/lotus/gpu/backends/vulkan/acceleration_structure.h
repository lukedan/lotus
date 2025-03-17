#pragma once

/// \file
/// Vulkan acceleration structures.

#include "details.h"

namespace lotus::gpu::backends::vulkan {
	class command_list;
	class device;


	/// Contains a \p vk::AccelerationStructureBuildGeometryInfoKHR and a list of
	/// \p vk::AccelerationStructureGeometryKHR.
	class bottom_level_acceleration_structure_geometry {
		friend command_list;
		friend device;
	protected:
		/// Initializes this object to empty.
		bottom_level_acceleration_structure_geometry(std::nullptr_t) {
		}
	private:
		// TODO allocator
		std::vector<vk::AccelerationStructureGeometryKHR> _geometries; ///< The list of geometries.
		// TODO allocator
		std::vector<u32> _pimitive_counts; ///< Primitive counts.
		vk::AccelerationStructureBuildGeometryInfoKHR _build_info; ///< Acceleration structure build info.
	};

	/// Contains a \p vk::AccelerationStructureInstanceKHR.
	class instance_description {
		friend device;
	protected:
		/// No initialization.
		instance_description(uninitialized_t) {
		}
	private:
		vk::AccelerationStructureInstanceKHR _instance; ///< Instance data.
	};

	/// Contains a \p vk::UniqueAccelerationStructureKHR.
	class bottom_level_acceleration_structure {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty acceleration structure.
		bottom_level_acceleration_structure(std::nullptr_t) {
		}

		/// Returns whether \ref _acceleration_structure is a valid handle.
		[[nodiscard]] bool is_valid() const {
			return _acceleration_structure.get();
		}
	private:
		/// The acceleration structure.
		vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> _acceleration_structure;
	};

	/// Contains a \p vk::UniqueAccelerationStructureKHR.
	class top_level_acceleration_structure {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty acceleration structure.
		top_level_acceleration_structure(std::nullptr_t) {
		}

		/// Returns whether \ref _acceleration_structure is a valid handle.
		[[nodiscard]] bool is_valid() const {
			return _acceleration_structure.get();
		}
	private:
		/// The acceleration structure.
		vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> _acceleration_structure;
	};
}
