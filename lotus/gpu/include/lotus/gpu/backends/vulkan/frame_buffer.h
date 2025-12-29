#pragma once

/// \file
/// Vulkan framebuffers and swapchains.

#include "details.h"
#include "resources.h"
#include "synchronization.h"

namespace lotus::gpu::backends::vulkan {
	class command_list;
	class command_queue;
	class context;
	class device;


	/// Contains a \p vk::UniqueSurfaceKHR and a \p vk::UniqueSwapchainKHR.
	class swap_chain {
		friend command_queue;
		friend context;
		friend device;
	public:
		/// Default move constructor.
		swap_chain(swap_chain&&) = default;
		/// Move assignment. Ensures the correct order of destruction.
		swap_chain &operator=(swap_chain &&src) noexcept {
			if (this != &src) {
				// we must destroy the swapchain *before* the surface
				_swapchain = std::exchange(src._swapchain, vk::UniqueSwapchainKHR());
				_surface = std::exchange(src._surface, vk::UniqueSurfaceKHR());

				_format = src._format;

				_images = std::move(src._images);
				_fences = std::move(src._fences);
				_image_to_present = src._image_to_present;
			}
			return *this;
		}
	protected:
		/// Creates an empty swapchain object.
		swap_chain(std::nullptr_t) {
		}

		/// Returns whether this object holds a valid swap chain.
		[[nodiscard]] bool is_valid() const {
			return !!_swapchain;
		}
	private:
		vk::UniqueSurfaceKHR _surface; ///< The surface of the window.
		vk::UniqueSwapchainKHR _swapchain; ///< The swapchain.

		vk::SurfaceFormatKHR _format; ///< The format of this swap chain.

		// TODO allocator
		std::vector<vk::Image> _images; ///< Images associated with this \ref swap_chain.
		/// Synchronization primitives for all back buffers.
		std::vector<fence> _fences;

		u16 _fence_to_signal = 0; ///< The next fence to wait on when presenting an image.
		u16 _image_to_present = 0; ///< Index of the image to present next.
	};

	/// Contains a \p vk::UniqueFramebuffer.
	class frame_buffer {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty object.
		frame_buffer(std::nullptr_t) {
		}
	private:
		std::vector<vk::ImageView> _color_views; ///< Color views.
		vk::ImageView _depth_stencil_view; ///< Deptn stencil view.
		cvec2u32 _size = zero; ///< The size of this frame buffer.
	};
}
