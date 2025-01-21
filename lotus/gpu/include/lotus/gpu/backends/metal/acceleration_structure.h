#pragma once

/// \file
/// Metal acceleration structures.

#include <cstddef>

namespace lotus::gpu::backends::metal {
	class command_list;
	class device;


	/// Contains a \p MTL::PrimitiveAccelerationStructureDescriptor.
	class bottom_level_acceleration_structure_geometry {
		friend command_list;
		friend device;
	protected:
		/// Initializes this object to empty.
		bottom_level_acceleration_structure_geometry(std::nullptr_t) {
		}
	private:
		/// Acceleration structure descriptor.
		NS::SharedPtr<MTL::PrimitiveAccelerationStructureDescriptor> _descriptor;

		/// Initializes \ref _descriptor.
		explicit bottom_level_acceleration_structure_geometry(
			NS::SharedPtr<MTL::PrimitiveAccelerationStructureDescriptor> desc
		) : _descriptor(std::move(desc)) {
		}
	};

	/// Contains a \p MTL::IndirectAccelerationStructureInstanceDescriptor.
	class instance_description {
		friend command_list;
		friend device;
	protected:
		/// No initialization.
		instance_description(uninitialized_t) {
		}
	private:
		MTL::IndirectAccelerationStructureInstanceDescriptor _descriptor; ///< The descriptor.
	};
	static_assert(
		sizeof(instance_description) == sizeof(MTL::IndirectAccelerationStructureInstanceDescriptor),
		"Acceleration structure instance descriptor size mismatch"
	);

	/// Holds a \p MTL::AccelerationStructure object.
	class bottom_level_acceleration_structure {
		friend command_list;
		friend device;
	protected:
		/// Initializes the object to empty.
		bottom_level_acceleration_structure(std::nullptr_t) {
		}

		/// Checks if the object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_as;
		}
	private:
		NS::SharedPtr<MTL::AccelerationStructure> _as; ///< The acceleration structure.

		/// Initializes \ref _as.
		explicit bottom_level_acceleration_structure(NS::SharedPtr<MTL::AccelerationStructure> as) :
			_as(std::move(as)) {
		}
	};

	/// Holds a \p MTL::AccelerationStructure object.
	class top_level_acceleration_structure {
		friend command_list;
		friend device;
	protected:
		/// Initializes the object to empty.
		top_level_acceleration_structure(std::nullptr_t) {
		}

		/// Checks if the object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_as;
		}
	private:
		NS::SharedPtr<MTL::AccelerationStructure> _as; ///< The acceleration structure.
		NS::SharedPtr<MTL::Buffer> _header; ///< The acceleration structure header buffer.

		/// Initializes all fields of this struct.
		top_level_acceleration_structure(
			NS::SharedPtr<MTL::AccelerationStructure> as, NS::SharedPtr<MTL::Buffer> header
		) : _as(std::move(as)), _header(std::move(header)) {
		}
	};
}
