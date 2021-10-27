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
		friend device;
	protected:
		/// Creates an empty object.
		swap_chain(std::nullptr_t) {
		}

		/// Returns the number of images in this swapchain.
		[[nodiscard]] std::size_t get_image_count() const {
			return _synchronization.size();
		}
		/// Calls \p IDXGISwapChain1::GetBuffer().
		[[nodiscard]] image2d get_image(std::size_t);
		/// Updates all elements in \ref _synchronization.
		void update_synchronization_primitives(std::span<const back_buffer_synchronization>);
	private:
		/// Cached synchronization primitives for a back buffer.
		struct _cached_back_buffer_synchronization {
			/// Initializes all fields to \p nullptr.
			_cached_back_buffer_synchronization(std::nullptr_t) : notify_fence(nullptr), next_fence(nullptr) {
			}

			graphics::fence *notify_fence; ///< Fence to be notified when this back buffer has finished presenting.
			graphics::fence *next_fence; ///< Fence for the next frame.
		};

		_details::com_ptr<IDXGISwapChain3> _swap_chain; ///< The swap chain.
		// TODO allocator
		std::vector<_cached_back_buffer_synchronization> _synchronization; ///< Synchronization for back buffers.
	};

	/// A number of \p D3D12_CPU_DESCRIPTOR_HANDLE objects that are used for color and depth-stencil descriptors.
	class frame_buffer {
		friend command_list;
		friend device;
	public:
		/// Destroys all descriptors.
		~frame_buffer();
	protected:
		/// Creates an empty object.
		frame_buffer(std::nullptr_t) : _color(nullptr), _depth_stencil(nullptr), _device(nullptr) {
		}
		/// Move construction.
		frame_buffer(frame_buffer&&) noexcept;
		/// Move assignment.
		frame_buffer &operator=(frame_buffer&&) noexcept;
	private:
		_details::descriptor_range _color; ///< Color descriptors.
		_details::descriptor_range _depth_stencil; ///< Depth stencil descriptor.
		device *_device; ///< The device that created this object.

		/// Initializes \ref _device.
		explicit frame_buffer(device &dev) : _color(nullptr), _depth_stencil(nullptr), _device(&dev) {
		}

		/// Frees all descriptors.
		void _free();
	};
}
