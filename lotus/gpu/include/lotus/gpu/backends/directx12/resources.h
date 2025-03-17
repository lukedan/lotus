#pragma once

/// \file
/// Buffers and textures.

#include "details.h"

namespace lotus::gpu::backends::directx12 {
	class command_list;
	class command_queue;
	class device;
	class swap_chain;


	/// A \p ID3D12Heap1.
	class memory_block {
		friend device;
	protected:
	private:
		_details::com_ptr<ID3D12Heap> _heap; ///< The heap.
	};


	/// A \p ID3D12Resource that represents a generic buffer.
	class buffer {
		friend bottom_level_acceleration_structure_geometry;
		friend command_list;
		friend device;
	protected:
		/// Creates an invalid buffer object.
		buffer(std::nullptr_t) {
		}

		/// Returns whether \ref _buffer is \p nullptr.
		[[nodiscard]] bool is_valid() const {
			return _buffer != nullptr;
		}
	private:
		_details::com_ptr<ID3D12Resource> _buffer; ///< The buffer.
		u32 _flush_maps = 0; ///< Outstanding map operations caused by flush operations.
	};


	namespace _details {
		/// Base class for images.
		class image_base : public gpu::image_base {
			friend command_list;
			friend device;
		protected:
			/// Returns whether \ref _image is valid.
			[[nodiscard]] bool is_valid() const {
				return _image;
			}

			_details::com_ptr<ID3D12Resource> _image; ///< The image.
		};
	}
	/// Base class containing a \p ID3D12Resource.
	template <image_type Type> class basic_image : public _details::image_base {
		friend device;
		friend swap_chain;
	protected:
		/// Initializes this object to empty.
		basic_image(std::nullptr_t) {
		}
	};
	using image2d = basic_image<image_type::type_2d>; ///< 2D images.


	namespace _details {
		/// Base class for image views.
		class image_view_base : public gpu::image_view_base {
			friend device;
		protected:
			/// Returns whether \ref _image is empty.
			[[nodiscard]] bool is_valid() const {
				return _image;
			}

			_details::com_ptr<ID3D12Resource> _image; ///< The image.
			D3D12_SHADER_RESOURCE_VIEW_DESC _srv_desc; ///< Shader resource view description.
			D3D12_UNORDERED_ACCESS_VIEW_DESC _uav_desc; ///< Unordered access view description.
		};
	}
	/// Base class of all image views.
	template <image_type Type> class basic_image_view : public _details::image_view_base {
		friend device;
	protected:
		/// Creates an empty image view object.
		basic_image_view(std::nullptr_t) {
		}
	};
	using image2d_view = basic_image_view<image_type::type_2d>; ///< 2D image views.


	/// Wraps around a \p D3D12_SAMPLER_DESC.
	class sampler {
		friend device;
	protected:
		/// Initializes this to a default sampler.
		sampler(std::nullptr_t) : _desc({}) {
		}
	private:
		D3D12_SAMPLER_DESC _desc;
	};
}
