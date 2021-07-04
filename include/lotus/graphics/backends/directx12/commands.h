#pragma once

/// \file
/// DirectX 12 command queues.

#include <d3d12.h>

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class device;

	/// A \p ID3D12CommandAllocator.
	class command_allocator {
		friend device;
	protected:

	private:
		_details::com_ptr<ID3D12CommandAllocator> _allocator; ///< The allocator.
	};
}
