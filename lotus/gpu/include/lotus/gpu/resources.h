#pragma once

/// \file
/// Buffers and textures.

#include LOTUS_GPU_BACKEND_INCLUDE

namespace lotus::gpu {
	class device;
	class swap_chain;


	/// A large block of memory that buffers and images can be allocated out of.
	class memory_block : public backend::memory_block {
		friend device;
	public:
		/// Move constructor.
		memory_block(memory_block &&src) : backend::memory_block(std::move(src)) {
		}
		/// No copy construction.
		memory_block(const memory_block&) = delete;
		/// Move assignment.
		memory_block &operator=(memory_block &&src) {
			backend::memory_block::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		memory_block &operator=(const memory_block&) = delete;
	protected:
		/// Initializes the base class.
		memory_block(backend::memory_block base) : backend::memory_block(std::move(base)) {
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

		/// Returns whether this object references a valid buffer.
		[[nodiscard]] bool is_valid() const {
			return backend::buffer::is_valid();
		}
		/// Shorthand for \ref is_valid.
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	protected:
		/// Initializes the base class.
		buffer(backend::buffer base) : backend::buffer(std::move(base)) {
		}
	};

	/// A \ref buffer used for uploading image data to the device.
	class staging_buffer {
	public:
		/// Initializes the buffer to empty without initializing \ref row_pitch or \ref total_size.
		staging_buffer(std::nullptr_t) : data(nullptr), meta(uninitialized) {
		}

		/// An opaque token used by backends to record additional metadata of the image.
		struct metadata : public backend::staging_buffer_metadata {
			friend device;
		public:
			/// No initialization.
			metadata(uninitialized_t) : backend::staging_buffer_metadata(uninitialized) {
			}

			/// Returns the pitch in bytes.
			std::size_t get_pitch_in_bytes() const {
				return backend::staging_buffer_metadata::get_pitch_in_bytes();
			}
		private:
			/// Initializes the base class object.
			metadata(backend::staging_buffer_metadata p) : backend::staging_buffer_metadata(std::move(p)) {
			}
		};

		buffer data; ///< The actual buffer.
		metadata meta; ///< Additional metadata.
		std::size_t total_size; ///< Total size of \ref data.
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

		/// Returns whether this is a valid image.
		[[nodiscard]] bool is_valid() const {
			return backend::image2d::is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
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

		/// Returns whether this holds a valid object.
		[[nodiscard]] bool is_valid() const {
			return backend::image2d_view::is_valid();
		}
		/// \override
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	protected:
		/// Initializes the base class.
		image2d_view(backend::image2d_view base) : backend::image2d_view(std::move(base)) {
		}
	};


	/// A sampler.
	class sampler : public backend::sampler {
		friend device;
	public:
		/// Initializes this sampler to empty.
		sampler(std::nullptr_t) : backend::sampler(nullptr) {
		}
		/// Move constructor.
		sampler(sampler &&src) : backend::sampler(std::move(src)) {
		}
		/// No copy construction.
		sampler(const sampler&) = delete;
		/// Move assignment.
		sampler &operator=(sampler &&src) {
			backend::sampler::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		sampler &operator=(const sampler&) = delete;
	protected:
		/// Initializes the base class.
		sampler(backend::sampler base) : backend::sampler(std::move(base)) {
		}
	};
}
