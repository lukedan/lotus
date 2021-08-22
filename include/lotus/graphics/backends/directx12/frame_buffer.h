#pragma once

/// \file
/// DirectX 12 swap chains.

#include <array>

#include <dxgi1_4.h>

#include "details.h"
#include "resources.h"

namespace lotus::graphics::backends::directx12 {
	class command_list;
	class command_queue;
	class context;
	class device;
	class fence;


	/// A \p IDXGISwapChain1.
	class swap_chain {
		friend context;
		friend command_queue;
	protected:
		/// Calls \p IDXGISwapChain1::GetBuffer().
		[[nodiscard]] image2d get_image(std::size_t);
		/// Calls \p IDXGISwapChain3::GetCurrentBackBufferIndex().
		[[nodiscard]] back_buffer_info acquire_back_buffer();
	private:
		_details::com_ptr<IDXGISwapChain3> _swap_chain; ///< The swap chain.
		// TODO allocator
		std::vector<graphics::fence*> _on_presented; ///< Fences that will be signaled when a frame has finished presenting.
	};

	/// A number of \p D3D12_CPU_DESCRIPTOR_HANDLE objects that are used for color and depth-stencil descriptors.
	class frame_buffer {
		friend command_list;
		friend device;
	public:
		/// Destroys all descriptors.
		~frame_buffer();
	protected:
		/// Initializes \ref _device.
		explicit frame_buffer(device *dev) :
			_device(dev),
			_color({ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }),
			_depth_stencil(nullptr) {
		}
		/// Move construction.
		frame_buffer(frame_buffer&&) noexcept = default;
		/// Move assignment.
		frame_buffer &operator=(frame_buffer&&) noexcept = default;
	private:
		device *_device; ///< The device that created this object.
		std::array<_details::descriptor, num_color_render_targets> _color; ///< Color descriptors.
		_details::descriptor _depth_stencil; ///< Depth stencil descriptor.
	};
}
