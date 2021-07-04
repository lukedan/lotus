#pragma once

/// \file
/// DirectX 12 descriptor heaps.

#include <array>

#include <d3d12.h>

namespace lotus::graphics::backends::directx12 {
	/// An array of \p ID3D12DescriptorHeap.
	class descriptor_pool {
	protected:
	private:
		_details::com_ptr<ID3D12DescriptorHeap> _resource_heaps; /// Heaps for all resources.
		_details::com_ptr<ID3D12DescriptorHeap> _sampler_heaps; /// Heaps for samplers.
	};
}
