#pragma once

/// \file
/// DirectX 12 descriptor heaps.

#include <vector>

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class command_list;
	class device;


	/// Since DirectX 12 disallows binding multiple descriptor heaps at the same time, this struct is simply for
	/// bookkeeping and the descriptor heaps are stored in the \ref device.
	class descriptor_pool {
		friend device;
	protected:
		/// Initializes this object to empty.
		descriptor_pool(std::nullptr_t) {
		}
	private:
		// TODO actually do bookkeeping
	};

	/// An array of \p D3D12_DESCRIPTOR_RANGE1 objects.
	class descriptor_set_layout {
		friend device;
	protected:
		/// Creates an invalid layout.
		descriptor_set_layout(std::nullptr_t) :
			_visibility(D3D12_SHADER_VISIBILITY_ALL),
			_num_shader_resource_descriptors(0), _num_sampler_descriptors(0),
			_num_shader_resource_ranges(std::numeric_limits<std::size_t>::max()) {
		}

		/// Checks if this layout is valid.
		[[nodiscard]] bool is_valid() const {
			return _num_shader_resource_ranges <= _ranges.size();
		}
	private:
		// TODO allocator
		std::vector<D3D12_DESCRIPTOR_RANGE1> _ranges; ///< Descriptor ranges.
		D3D12_SHADER_VISIBILITY _visibility; ///< Visibility to various shader stages.
		/// The number of shader resource descriptors. Does not include any range with unbounded size.
		UINT _num_shader_resource_descriptors;
		/// The number of sampler descriptors. Does not include any range with unbounded size.
		UINT _num_sampler_descriptors;
		/// The number of ranges in \ref _ranges that contain shader resources. If this is larger than the size of
		/// \ref _ranges, it indicates that this layout has not been properly initialized.
		std::size_t _num_shader_resource_ranges;
		bool _unbounded_range_is_sampler; ///< If there is a range with unbounded size, whether it's for samplers.

		/// Finds the descriptor range that corresponds to the given register range, and makes sure that it's valid.
		[[nodiscard]] std::vector<D3D12_DESCRIPTOR_RANGE1>::const_iterator _find_register_range(
			D3D12_DESCRIPTOR_RANGE_TYPE, std::size_t first_reg, std::size_t num_regs
		) const;
	};

	/// An array of descriptors.
	class descriptor_set {
		friend command_list;
		friend device;
	public:
		/// Creates an empty object.
		descriptor_set(std::nullptr_t) :
			_shader_resource_descriptors(nullptr), _sampler_descriptors(nullptr), _device(nullptr) {
		}
		/// Frees the descriptors if necessary.
		~descriptor_set();
	protected:
		/// Move constructor.
		descriptor_set(descriptor_set&&) noexcept;
		/// Move assignment.
		descriptor_set &operator=(descriptor_set&&) noexcept;

		/// Returns whether this descriptor set is valid.
		[[nodiscard]] bool is_valid() const {
			return _device;
		}
	private:
		_details::descriptor_range _shader_resource_descriptors; ///< Shader resource descriptors.
		_details::descriptor_range _sampler_descriptors; ///< Sampler descriptors.
		device *_device; ///< The device that created this descriptor set.

		/// Initializes \ref _device.
		explicit descriptor_set(device &dev) :
			_shader_resource_descriptors(nullptr), _sampler_descriptors(nullptr), _device(&dev) {
		}

		/// Frees all descriptors if necessary.
		void _free();
	};
}
