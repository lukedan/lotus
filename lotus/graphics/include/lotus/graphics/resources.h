#pragma once

/// \file
/// Buffers and textures.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class device;
	class swap_chain;


	/// A large block of memory that buffers and images can be allocated out of.
	class device_heap : public backend::device_heap {
		friend device;
	public:
		/// No copy construction.
		device_heap(const device_heap&) = delete;
		/// No copy assignment.
		device_heap &operator=(const device_heap&) = delete;
	protected:
		/// Initializes the base class.
		device_heap(backend::device_heap base) : backend::device_heap(std::move(base)) {
		}
	};


	/// A generic buffer.
	class buffer : public backend::buffer {
		friend device;
	public:
		/// Does not create a buffer object.
		buffer(std::nullptr_t) : backend::buffer(nullptr) {
		}
		/// Move constructor.
		buffer(buffer &&src) noexcept : backend::buffer(std::move(src)) {
		}
		/// No copy construction.
		buffer(const buffer&) = delete;
		/// Move assignment.
		buffer &operator=(buffer &&src) noexcept {
			backend::buffer::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		buffer &operator=(const buffer&) = delete;
	protected:
		/// Initializes the base class.
		buffer(backend::buffer base) : backend::buffer(std::move(base)) {
		}
	};


	/// 2D images.
	class image2d : public backend::image2d {
		friend device;
		friend swap_chain;
	public:
		static_assert(
			std::is_base_of_v<image, backend::image2d>,
			"Image types must be derived from the base image type"
		);

		/// Creates an empty object.
		image2d(std::nullptr_t) : backend::image2d(nullptr) {
		}
		/// Move constructor.
		image2d(image2d &&src) noexcept : backend::image2d(std::move(src)) {
		}
		/// No copy construction.
		image2d(const image2d&) = delete;
		/// Move assignment.
		image2d &operator=(image2d &&src) noexcept {
			backend::image2d::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		image2d &operator=(const image2d&) = delete;
	protected:
		/// Initializes the base class.
		image2d(backend::image2d base) : backend::image2d(std::move(base)) {
		}
	};


	/// 2D image views.
	class image2d_view : public backend::image2d_view {
		friend device;
	public:
		static_assert(
			std::is_base_of_v<image_view, backend::image2d_view>,
			"Image types must be derived from the base image type"
		);

		/// Initializes this view to an empty object.
		image2d_view(std::nullptr_t) : backend::image2d_view(nullptr) {
		}
		/// Move constructor.
		image2d_view(image2d_view &&src) noexcept : backend::image2d_view(std::move(src)) {
		}
		/// No copy construction.
		image2d_view(const image2d_view&) = delete;
		/// Move assignment.
		image2d_view &operator=(image2d_view &&src) noexcept {
			backend::image2d_view::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		image2d_view &operator=(const image2d_view&) = delete;
	protected:
		/// Initializes the base class.
		image2d_view(backend::image2d_view base) : backend::image2d_view(std::move(base)) {
		}
	};


	/// A sampler.
	class sampler : public backend::sampler {
		friend device;
	public:
		/// No copy construction.
		sampler(const sampler&) = delete;
		/// No copy assignment.
		sampler &operator=(const sampler&) = delete;
	protected:
		/// Initializes the base class.
		sampler(backend::sampler base) : backend::sampler(std::move(base)) {
		}
	};
}
