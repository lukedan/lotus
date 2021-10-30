#pragma once

/// \file
/// Interface to swap chains.

#include "lotus/utils/stack_allocator.h"
#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "resources.h"

namespace lotus::graphics {
	class context;


	/// Swap chain used for presenting to screens.
	class swap_chain : public backend::swap_chain {
		friend context;
	public:
		/// Creates an empty object.
		swap_chain(std::nullptr_t) : backend::swap_chain(nullptr) {
		}
		/// Move constructor.
		swap_chain(swap_chain &&base) : backend::swap_chain(std::move(base)) {
		}
		/// No copy construction.
		swap_chain(const swap_chain&) = delete;
		/// Move assignment.
		swap_chain &operator=(swap_chain &&base) {
			backend::swap_chain::operator=(std::move(base));
			return *this;
		}
		/// No copy assignment.
		swap_chain &operator=(const swap_chain&) = delete;

		/// Returns the actual number of images in this swapchain.
		[[nodiscard]] std::size_t get_image_count() const {
			return backend::swap_chain::get_image_count();
		}
		/// Returns the backing image at the given index.
		[[nodiscard]] image2d get_image(std::size_t index) {
			return backend::swap_chain::get_image(index);
		}
		/// Updates the synchronization primitives used internally. This will affect the next frame for which
		/// \ref command_queue::present() has not been called. There should be exactly \ref get_image_count()
		/// elements in the array, although they may not correspond one-to-one to each swapchain image.
		void update_synchronization_primitives(std::span<const back_buffer_synchronization> prim) {
			backend::swap_chain::update_synchronization_primitives(prim);
		}
		/// \overload
		void update_synchronization_primitives(std::initializer_list<back_buffer_synchronization> prim) {
			update_synchronization_primitives({ prim.begin(), prim.end() });
		}
		/// \overload
		void update_synchronization_primitives(std::span<fence> fences) {
			auto bookmark = stack_allocator::for_this_thread().bookmark();
			auto prims = bookmark.create_vector_array<back_buffer_synchronization>(get_image_count(), nullptr);
			assert(fences.empty() || fences.size() == prims.size());
			for (std::size_t i = 0; i < fences.size(); ++i) {
				prims[i].notify_fence = &fences[i];
			}
			update_synchronization_primitives(prims);
		}
	protected:
		/// Initializes the backend swap chain.
		swap_chain(backend::swap_chain base) : backend::swap_chain(std::move(base)) {
		}
	};

	/// A frame buffer that can be rendered to.
	class frame_buffer : public backend::frame_buffer {
		friend device;
	public:
		/// Creates an empty \ref frame_buffer.
		frame_buffer(std::nullptr_t) : backend::frame_buffer(nullptr) {
		}
		/// Move constructor.
		frame_buffer(frame_buffer &&src) : backend::frame_buffer(std::move(src)) {
		}
		/// No copy construction.
		frame_buffer(const frame_buffer&) = delete;
		/// Move assignment
		frame_buffer &operator=(frame_buffer &&src) {
			backend::frame_buffer::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		frame_buffer &operator=(const frame_buffer&) = delete;
	protected:
		/// Initializes the base class.
		frame_buffer(backend::frame_buffer base) : backend::frame_buffer(std::move(base)) {
		}
	};
}
