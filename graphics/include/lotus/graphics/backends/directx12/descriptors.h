#pragma once

/// \file
/// DirectX 12 descriptor heaps.

#include <vector>

#include <d3d12.h>

namespace lotus::graphics::backends::directx12 {
	class device;


	/// Contains a \p ID3D12DescriptorHeap.
	class descriptor_pool {
		friend device;
	protected:
	private:
		/// The descriptor heap for shader resources (\p D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).
		_details::com_ptr<ID3D12DescriptorHeap> _shader_resource_heap;
		/// The descriptor heap for samplers (\p D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).
		_details::com_ptr<ID3D12DescriptorHeap> _sampler_heap;
	};

	/// An array of \p D3D12_DESCRIPTOR_RANGE1 objects.
	class descriptor_set_layout {
		friend device;
	protected:
	private:
		// TODO allocator
		std::vector<D3D12_DESCRIPTOR_RANGE1> _ranges; ///< Descriptor ranges.
		D3D12_SHADER_VISIBILITY _visibility; ///< Visibility to various shader stages.
	};
}
