#pragma once

/// \file
/// DirectX 12 descriptor heaps.

#include <array>

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
}
