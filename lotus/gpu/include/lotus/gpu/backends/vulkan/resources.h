#pragma once

/// \file
/// Resources.

#include <vulkan/vulkan.hpp>

#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::vulkan {
	class command_list;
	class device;
	class swap_chain;


	namespace _details {
		/// Stores data of a memory block that can be shared between resources.
		class memory_block {
		public:
			/// Initializes this memory block.
			explicit memory_block(vk::UniqueDeviceMemory m) : _memory(std::move(m)) {
			}

			/// Maps this memory block if necessary, and returns the starting address.
			[[nodiscard]] std::byte *map();
			/// Unmaps this memory block if necessary.
			void unmap();

			/// Returns the memory object.
			[[nodiscard]] vk::DeviceMemory get_memory() const {
				return _memory.get();
			}
		private:
			vk::UniqueDeviceMemory _memory; ///< The vulkan memory block.
			std::uint32_t _num_maps = 0; ///< The number of users that have mapped a resource in this memory block.
			std::byte *_mapped_addr = nullptr; ///< Mapped address.
		};
	}

	/// Contains a \p vk::UniqueDeviceMemory.
	class memory_block {
		friend device;
	protected:
	private:
		std::unique_ptr<_details::memory_block> _memory; ///< The memory block.
	};

	/// Contains a \p vk::Buffer, its associated \p vk::DeviceMemory if present, and the associated \p vk::Device.
	class buffer {
		friend command_list;
		friend device;
	public:
		/// Returns whether \ref _buffer is empty.
		[[nodiscard]] bool is_valid() const {
			return !!_buffer;
		}
	protected:
		/// Creates an empty object.
		buffer(std::nullptr_t) {
		}
	private:
		_details::memory_block *_memory = nullptr; ///< The memory block that contains this buffer.
		// TODO these two are mutually exclusive - maybe we can save space somehow?
		/// The memory block owned by this buffer. This is valid if this is a committed buffer, and will be the same
		/// as \ref _memory.
		std::unique_ptr<_details::memory_block> _committed_memory;
		std::size_t _base_offset = 0; ///< Offset of this buffer within the memory block.
		vk::UniqueBuffer _buffer; ///< The buffer.
	};

	/// Stores additional information about a staging buffer.
	struct staging_buffer_metadata {
		friend command_list;
		friend device;
	public:
		/// No initialization.
		staging_buffer_metadata(uninitialized_t) : _size(uninitialized) {
		}
	protected:
		/// Returns \ref _bytes.
		std::size_t get_pitch_in_bytes() const {
			return _bytes;
		}
	private:
		cvec2u32 _size; ///< Size of the texture in pixels.
		std::uint32_t _bytes; ///< The number of bytes between two consecutive rows.
		gpu::format _format; ///< Image data format.
	};

	namespace _details {
		/// Base class of all image types, contains a \p vk::Image and the \p vk::Device that created it.
		class image_base : public gpu::image_base {
			friend command_list;
			friend device;
			friend swap_chain;
		public:
			/// Move construction.
			image_base(image_base &&src) :
				_device(std::exchange(src._device, nullptr)),
				_memory(std::exchange(src._memory, nullptr)),
				_image(std::exchange(src._image, nullptr)) {
			}
			/// No copy construction.
			image_base(const image_base&) = delete;
			/// Move assignment.
			image_base &operator=(image_base &&src) {
				if (&src != this) {
					_free();
					_device = std::exchange(src._device, nullptr);
					_memory = std::exchange(src._memory, nullptr);
					_image = std::exchange(src._image, nullptr);
				}
				return *this;
			}
			/// No copy assignment.
			image_base &operator=(const image_base&) = delete;
			/// Calls \ref _free().
			~image_base() {
				_free();
			}
		protected:
			/// Creates an empty object.
			image_base(std::nullptr_t) {
			}

			/// Returns whether this refers to a valid image object.
			[[nodiscard]] bool is_valid() const {
				return !!_image;
			}
		private:
			/// The device. If this \p nullptr, it means that the image is not owned by this object and does not need
			/// to be destroyed when this object is disposed.
			vk::Device _device;
			vk::DeviceMemory _memory; ///< Memory dedicated for this image.
			vk::Image _image; ///< The image.

			/// Frees \ref _image, and \ref _memory if necessary.
			void _free() {
				if (_device) {
					// TODO allocator
					_device.destroyImage(_image);
					if (_memory) {
						_device.freeMemory(_memory);
					}
				}
			}
		};

		/// Base class of all image view types - contains a \p vk::UniqueImageView.
		class image_view_base : public gpu::image_view_base {
			friend device;
		protected:
			/// Creates an empty object.
			image_view_base(std::nullptr_t) {
			}

			/// Returns whether \ref _view is empty.
			[[nodiscard]] bool is_valid() const {
				return static_cast<bool>(_view);
			}
		private:
			vk::UniqueImageView _view; ///< The image view.
		};
	}

	/// A 2D image.
	template <image_type Type> class basic_image : public _details::image_base {
		friend swap_chain;
		friend device;
	protected:
		/// Creates an empty object.
		basic_image(std::nullptr_t) : _details::image_base(nullptr) {
		}
	};
	using image2d = basic_image<image_type::type_2d>; ///< 2D images.

	/// A 2D image view.
	template <image_type Type> class basic_image_view : public _details::image_view_base {
		friend device;
	protected:
		/// Creates an empty object.
		basic_image_view(std::nullptr_t) : _details::image_view_base(nullptr) {
		}
	};
	using image2d_view = basic_image_view<image_type::type_2d>; ///< 2D image views.

	/// Contains a \p vk::UniqueSampler.
	class sampler {
		friend device;
	protected:
		/// Initializes this sampler to empty.
		sampler(std::nullptr_t) {
		}
	private:
		vk::UniqueSampler _sampler; ///< The sampler.
	};
}
