#pragma once

/// \file
/// Buffers and textures.

#include LOTUS_GPU_BACKEND_INCLUDE_COMMON
#include LOTUS_GPU_BACKEND_INCLUDE_RESOURCES

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
			[[nodiscard]] std::size_t get_pitch_in_bytes() const {
				return backend::staging_buffer_metadata::get_pitch_in_bytes();
			}
			/// Returns the size of the image in pixels.
			[[nodiscard]] cvec2u32 get_size() const {
				return backend::staging_buffer_metadata::get_size();
			}
			/// Returns the target pixel format of this staging buffer.
			[[nodiscard]] gpu::format get_format() const {
				return backend::staging_buffer_metadata::get_format();
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
	template <image_type Type> class basic_image : public backend::basic_image<Type> {
		friend device;
		friend swap_chain;
	public:
		static_assert(
			std::is_base_of_v<gpu::image_base, backend::basic_image<Type>>,
			"Backend image types must derive from the base image type"
		);

		/// Creates an empty object.
		basic_image(std::nullptr_t) : backend::basic_image<Type>(nullptr) {
		}
		/// Move constructor.
		basic_image(basic_image &&src) noexcept : backend::basic_image<Type>(std::move(src)) {
		}
		/// No copy construction.
		basic_image(const basic_image&) = delete;
		/// Move assignment.
		basic_image &operator=(basic_image &&src) noexcept {
			backend::basic_image<Type>::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		basic_image &operator=(const basic_image&) = delete;

		/// Returns whether this is a valid image.
		[[nodiscard]] bool is_valid() const {
			return backend::basic_image<Type>::is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	protected:
		/// Initializes the base class.
		basic_image(backend::basic_image<Type> base) : backend::basic_image<Type>(std::move(base)) {
		}
	};


	/// 2D image views.
	template <image_type Type> class basic_image_view : public backend::basic_image_view<Type> {
		friend device;
	public:
		static_assert(
			std::is_base_of_v<gpu::image_view_base, backend::basic_image_view<Type>>,
			"Backend image view types must be derived from the base image view type"
		);

		/// Initializes this view to an empty object.
		basic_image_view(std::nullptr_t) : backend::basic_image_view<Type>(nullptr) {
		}
		/// Move constructor.
		basic_image_view(basic_image_view &&src) noexcept : backend::basic_image_view<Type>(std::move(src)) {
		}
		/// No copy construction.
		basic_image_view(const basic_image_view&) = delete;
		/// Move assignment.
		basic_image_view &operator=(basic_image_view &&src) noexcept {
			backend::basic_image_view<Type>::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		basic_image_view &operator=(const basic_image_view&) = delete;

		/// Returns whether this holds a valid object.
		[[nodiscard]] bool is_valid() const {
			return backend::basic_image_view<Type>::is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	protected:
		/// Initializes the base class.
		basic_image_view(backend::basic_image_view<Type> base) : backend::basic_image_view<Type>(std::move(base)) {
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
