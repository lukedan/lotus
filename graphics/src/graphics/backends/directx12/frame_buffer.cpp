#include "lotus/graphics/backends/directx12/frame_buffer.h"

/// \file
/// Implementation of DirectX 12 swap chains.

#include "lotus/graphics/backends/directx12/device.h"

namespace lotus::graphics::backends::directx12 {
	image2d swap_chain::get_image(std::size_t index) {
		image2d result = nullptr;
		_details::assert_dx(_swap_chain->GetBuffer(static_cast<UINT>(index), IID_PPV_ARGS(&result._image)));
		return result;
	}

	back_buffer_info swap_chain::acquire_back_buffer() {
		back_buffer_info result = uninitialized;
		result.index = static_cast<std::size_t>(_swap_chain->GetCurrentBackBufferIndex());
		result.on_presented = _on_presented[result.index];
		return result;
	}


	frame_buffer::~frame_buffer() {
		_device->_rtv_descriptors.free(std::move(_color));
		if (!_depth_stencil.is_empty()) {
			_device->_dsv_descriptors.free(std::move(_depth_stencil));
		}
	}
}
