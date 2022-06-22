#pragma once

/// \file
/// Vulkan framebuffers and swapchains.

#include "details.h"
#include "resources.h"
#include "synchronization.h"

namespace lotus::graphics::backends::vulkan {
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
		/// Ensures the correct order of destruction.
		swap_chain &operator=(swap_chain &&src) {
			if (this != &src) {
				// we must destroy the swapchain *before* the surface
				_swapchain = std::exchange(src._swapchain, vk::UniqueSwapchainKHR());
				_surface = std::exchange(src._surface, vk::UniqueSurfaceKHR());

				_format = src._format;

				_images = std::move(src._images);
				_synchronization = std::move(src._synchronization);
				_frame_counter = src._frame_counter;
				_frame_index = src._frame_index;
			}
			return *this;
		}
	protected:
		/// Creates an empty swapchain object.
		swap_chain(std::nullptr_t) {
		}

		/// Returns the size of \ref _images.
		[[nodiscard]] std::size_t get_image_count() const {
			return _images.size();
		}
		/// Returns the corresponding entry in \ref _images.
		[[nodiscard]] image2d get_image(std::size_t index);
		/// Updates \ref _synchronization.
		void update_synchronization_primitives(std::span<const back_buffer_synchronization>);

		/// Returns whether this object holds a valid swap chain.
		[[nodiscard]] bool is_valid() const {
			return _swapchain.get();
		}
	private:
		/// Synchronization primitives that will be notified when a frame has finished presenting.
		struct _cached_back_buffer_synchronization {
			/// Initializes all members to \p nullptr.
			_cached_back_buffer_synchronization(std::nullptr_t) : notify_fence(nullptr), next_fence(nullptr) {
			}

			/// The fence to notify. This is overwritten by \ref next_fence when \ref command_queue::present() is
			/// called, and stays valid until it is returned by \ref device::acquire_back_buffer().
			graphics::fence *notify_fence;
			graphics::fence *next_fence; ///< Fence to use for the next frame.
		};

		vk::UniqueSurfaceKHR _surface; ///< The surface of the window.
		vk::UniqueSwapchainKHR _swapchain; ///< The swapchain.

		vk::SurfaceFormatKHR _format; ///< The format of this swap chain.

		// TODO allocator
		std::vector<vk::Image> _images; ///< Images associated with this \ref swap_chain.
		/// Synchronization primitives for all back buffers.
		std::vector<_cached_back_buffer_synchronization> _synchronization;
		std::uint16_t _frame_counter = 0; ///< Frame counter module number of buffers.
		std::uint16_t _frame_index = 0; ///< Index of frame to present next.
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
		cvec2s _size = zero; ///< The size of this frame buffer.
	};
}
