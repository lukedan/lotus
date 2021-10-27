#pragma once

/// \file
/// Buffers and textures.

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class command_list;
	class command_queue;
	class device;
	class swap_chain;


	/// A \p ID3D12Heap1.
	class device_heap {
		friend device;
	protected:
	private:
		_details::com_ptr<ID3D12Heap> _heap; ///< The heap.
	};


	/// A \p ID3D12Resource that represents a generic buffer.
	class buffer {
		friend command_list;
		friend device;
	protected:
		/// Creates an invalid buffer object.
		buffer(std::nullptr_t) {
		}
	private:
		_details::com_ptr<ID3D12Resource> _buffer; ///< The buffer.
	};


	namespace _details {
		/// Base class containing a \p ID3D12Resource.
		class image : public graphics::image {
			friend command_list;
			friend device;
		protected:
			_details::com_ptr<ID3D12Resource> _image; ///< The image.
		};
	}


	/// A \p ID3D12Resource that represents a 2D image.
	class image2d : public _details::image {
		friend device;
		friend swap_chain;
	protected:
		/// Creates an empty object.
		image2d(std::nullptr_t) {
		}
	};


	namespace _details {
		/// Base class of all image views.
		class image_view : public graphics::image_view {
			friend device;
		protected:
			_details::com_ptr<ID3D12Resource> _image; ///< The image.
			D3D12_SHADER_RESOURCE_VIEW_DESC _desc; ///< Resource view description of this view.
		};
	}

	/// Wraps around a \p D3D12_TEX2D_SRV.
	class image2d_view : public _details::image_view {
		friend device;
	protected:
		/// Creates an empty image view object.
		image2d_view(std::nullptr_t) {
		}
	private:
		constexpr static D3D12_SRV_DIMENSION _srv_dimension = D3D12_SRV_DIMENSION_TEXTURE2D; ///< SRV dimension.
		constexpr static D3D12_RTV_DIMENSION _rtv_dimension = D3D12_RTV_DIMENSION_TEXTURE2D; ///< RTV dimension.

		/// Fills a \p D3D12_RENDER_TARGET_VIEW_DESC.
		void _fill_rtv_desc(D3D12_RENDER_TARGET_VIEW_DESC &desc) const {
			assert(_desc.Texture2D.MipLevels == 1);
			desc.Format               = _desc.Format;
			desc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice   = _desc.Texture2D.MostDetailedMip;
			desc.Texture2D.PlaneSlice = _desc.Texture2D.PlaneSlice;
		}
		/// Fills a \p D3D12_DEPTH_STENCIL_VIEW_DESC.
		void _fill_dsv_desc(D3D12_DEPTH_STENCIL_VIEW_DESC &desc) const {
			assert(_desc.Texture2D.MipLevels == 1);
			desc.Format             = _desc.Format;
			desc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
			desc.Flags              = D3D12_DSV_FLAG_NONE;
			desc.Texture2D.MipSlice = _desc.Texture2D.MostDetailedMip;
		}
	};


	/// Wraps around a \p D3D12_SAMPLER_DESC.
	class sampler {
		friend device;
	private:
		D3D12_SAMPLER_DESC _desc;
	};
}
