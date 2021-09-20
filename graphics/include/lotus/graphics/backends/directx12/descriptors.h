#pragma once

/// \file
/// DirectX 12 descriptor heaps.

#include <vector>

#include <d3d12.h>

namespace lotus::graphics::backends::directx12 {
	class command_list;
	class device;


	/// Since DirectX 12 disallows binding multiple descriptor heaps at the same time, this struct is simply for
	/// bookkeeping and the descriptor heaps are stored in the \ref device.
	class descriptor_pool {
		friend device;
	protected:
	private:
		// TODO actually do bookkeeping
	};

	/// An array of \p D3D12_DESCRIPTOR_RANGE1 objects.
	class descriptor_set_layout {
		friend device;
	protected:
	private:
		// TODO allocator
		std::vector<D3D12_DESCRIPTOR_RANGE1> _ranges; ///< Descriptor ranges.
		D3D12_SHADER_VISIBILITY _visibility; ///< Visibility to various shader stages.
		UINT _num_shader_resource_descriptors; ///< The number of shader resource descriptors.
		UINT _num_sampler_descriptors; ///< The number of sampler descriptors.
		/// The number of ranges in \ref _ranges that contain shader resources.
		std::size_t _num_shader_resource_ranges;

		/// Finds the descriptor range that corresponds to the given register range, and makes sure that it's valid.
		[[nodiscard]] std::vector<D3D12_DESCRIPTOR_RANGE1>::const_iterator _find_register_range(
			D3D12_DESCRIPTOR_RANGE_TYPE type, std::size_t first_reg, std::size_t num_regs
		) const {
			auto range_it = std::lower_bound(
				_ranges.begin(), _ranges.end(), first_reg,
				[type](const D3D12_DESCRIPTOR_RANGE1 &range, std::size_t reg) {
					if (range.RangeType != type) {
						return range.BaseShaderRegister + range.NumDescriptors < reg + 1;
					}
					return range.RangeType < type;
				}
			);
			assert(range_it != _ranges.end());
			assert(range_it->BaseShaderRegister <= first_reg);
			assert(range_it->BaseShaderRegister + range_it->NumDescriptors >= first_reg + num_regs);
			return range_it;
		}
	};

	/// An array of descriptors.
	class descriptor_set {
		friend command_list;
		friend device;
	protected:
	private:
		_details::descriptor_range _shader_resource_descriptors = nullptr; ///< Shader resource descriptors.
		_details::descriptor_range _sampler_descriptors = nullptr; ///< Sampler descriptors.
	};
}
