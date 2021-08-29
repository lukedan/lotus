#pragma once

/// \file
/// Interface to swap chains.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE
#include "resources.h"

namespace lotus::graphics {
	class context;


	/// Swap chain used for presenting to screens.
	class swap_chain : public backend::swap_chain {
		friend context;
	public:
		/// No copy construction.
		swap_chain(const swap_chain&) = delete;
		/// No copy assignment.
		swap_chain &operator=(const swap_chain&) = delete;

		/// Returns the backing image at the given index.
		[[nodiscard]] image2d get_image(std::size_t index) {
			return backend::swap_chain::get_image(index);
		}
		/// Acquires the next back buffer and returns its index in this swap chain. This should only be called once
		/// per frame.
		[[nodiscard]] back_buffer_info acquire_back_buffer() {
			return backend::swap_chain::acquire_back_buffer();
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
