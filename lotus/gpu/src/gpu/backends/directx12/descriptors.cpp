#include "lotus/gpu/backends/directx12/descriptors.h"

/// \file
/// Descriptor implementations.

#include "lotus/gpu/backends/directx12/device.h"

namespace lotus::gpu::backends::directx12 {
	std::vector<D3D12_DESCRIPTOR_RANGE1>::const_iterator descriptor_set_layout::_find_register_range(
		D3D12_DESCRIPTOR_RANGE_TYPE type, std::size_t first_reg, std::size_t num_regs
	) const {
		auto range_it = std::lower_bound(
			_ranges.begin(), _ranges.end(), first_reg,
			[type](const D3D12_DESCRIPTOR_RANGE1 &range, std::size_t reg) {
				if (range.RangeType == type) {
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


	descriptor_set::~descriptor_set() {
		_free();
	}

	descriptor_set::descriptor_set(descriptor_set &&src) noexcept :
		_shader_resource_descriptors(std::move(src._shader_resource_descriptors)),
		_sampler_descriptors(std::move(src._sampler_descriptors)),
		_device(std::exchange(src._device, nullptr)) {
	}

	descriptor_set &descriptor_set::operator=(descriptor_set &&src) noexcept {
		_free();
		_shader_resource_descriptors = std::move(src._shader_resource_descriptors);
		_sampler_descriptors = std::move(src._sampler_descriptors);
		_device = std::exchange(src._device, nullptr);
		return *this;
	}

	void descriptor_set::_free() {
		if (_device) {
			if (!_shader_resource_descriptors.is_empty()) {
				_device->_srv_descriptors.free(std::move(_shader_resource_descriptors));
			}
			if (!_sampler_descriptors.is_empty()) {
				_device->_sampler_descriptors.free(std::move(_sampler_descriptors));
			}
		}
	}
}
