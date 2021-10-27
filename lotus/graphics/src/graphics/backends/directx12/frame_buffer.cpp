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

	void swap_chain::update_synchronization_primitives(std::span<const back_buffer_synchronization> prim) {
		assert(prim.size() == _synchronization.size());
		for (std::size_t i = 0; i < prim.size(); ++i) {
			_synchronization[i].next_fence = prim[i].notify_fence;
		}
	}


	frame_buffer::~frame_buffer() {
		_free();
	}

	frame_buffer::frame_buffer(frame_buffer &&src) noexcept :
		_color(std::move(src._color)), _depth_stencil(std::move(src._depth_stencil)),
		_device(std::exchange(src._device, nullptr)) {
	}

	frame_buffer &frame_buffer::operator=(frame_buffer &&src) noexcept {
		_free();
		_device = std::exchange(src._device, nullptr);
		_color = std::move(src._color);
		_depth_stencil = std::move(src._depth_stencil);
		return *this;
	}

	void frame_buffer::_free() {
		if (_device) {
			_device->_rtv_descriptors.free(std::move(_color));
			if (!_depth_stencil.is_empty()) {
				_device->_dsv_descriptors.free(std::move(_depth_stencil));
			}
		}
	}
}
