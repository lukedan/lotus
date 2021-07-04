#pragma once

/// \file
/// DirectX 12 swap chains.

#include <dxgi1_2.h>

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class context;

	/// A swap chain.
	class swap_chain {
		friend context;
	protected:
	private:
		_details::com_ptr<IDXGISwapChain1> _swap_chain; ///< The swap chain.
		_details::com_ptr<ID3D12DescriptorHeap> _descriptor_heap; ///< Descriptor heap for the render target views.
	};
}
