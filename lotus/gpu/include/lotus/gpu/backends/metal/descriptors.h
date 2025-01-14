#pragma once

/// \file
/// Metal descriptors.

#include <cstddef>
#include <vector>

#include <Metal/Metal.hpp>

namespace lotus::gpu::backends::metal {
	class command_list;
	class device;


	/// Holds a \ref MTL::Heap that argument buffers are allocated out of.
	class descriptor_pool {
		friend device;
	protected:
		/// Initializes the object to empty.
		descriptor_pool(std::nullptr_t) {
		}
	private:
		NS::SharedPtr<MTL::Heap> _heap; ///< The memory heap.

		/// Initializes \ref _heap.
		explicit descriptor_pool(NS::SharedPtr<MTL::Heap> heap) : _heap(std::move(heap)) {
		}
	};

	/// Contains a list of descriptor range bindings.
	class descriptor_set_layout {
		friend device;
	protected:
		/// Initializes this object to empty.
		descriptor_set_layout(std::nullptr_t) {
		}

		/// \ref _stage being \ref shader_stage::num_enumerators indicates that this object is empty.
		[[nodiscard]] bool is_valid() const {
			return _stage != shader_stage::num_enumerators;
		}
	private:
		std::vector<descriptor_range_binding> _bindings; ///< The list of bindings.
		shader_stage _stage = shader_stage::num_enumerators; ///< The shader stage.
	};

	/// Contains a \p MTL::Buffer to be used as an argument buffer.
	class descriptor_set {
		friend command_list;
		friend device;
	protected:
		/// Initializes this object to empty.
		descriptor_set(std::nullptr_t) {
		}

		/// Checks whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_arg_buffer;
		}
	private:
		NS::SharedPtr<MTL::Buffer> _arg_buffer; ///< The argument buffer.
		std::vector<NS::SharedPtr<MTL::Resource>> _resources; ///< List of resources used in the argument buffer.

		/// Initializes \ref _arg_buffer and \ref _resources.
		descriptor_set(NS::SharedPtr<MTL::Buffer> arg_buffer, std::size_t num_slots) :
			_arg_buffer(std::move(arg_buffer)), _resources(num_slots) {
		}
	};
}
