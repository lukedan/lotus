#pragma once

/// \file
/// DirectX 12 synchronization primitives.

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class command_queue;
	class device;


	/// Contains a \p ID3D12Fence.
	class fence {
		friend command_queue;
		friend device;
	protected:
		/// Creates an empty fence.
		fence(std::nullptr_t) {
		}
	private:
		_details::com_ptr<ID3D12Fence> _fence; ///< Pointer to the fence.
	};

	/// Contains a \p ID3D12Fence.
	class timeline_semaphore {
		friend command_queue;
		friend device;
	protected:
		/// Initializes this to an empty object.
		timeline_semaphore(std::nullptr_t) {
		}
	private:
		_details::com_ptr<ID3D12Fence> _semaphore; ///< Pointer to the semaphore.
	};
}
