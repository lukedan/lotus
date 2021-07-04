#pragma once

/// \file
/// DirectX 12 devices and adapters.

#include <utility>

#include <d3d12.h>

#include "lotus/graphics/common.h"
#include "details.h"
#include "commands.h"
#include "swap_chain.h"
#include "descriptors.h"

namespace lotus::graphics::backends::directx12 {
	class adapter;
	class context;
	class device;

	/// A DirectX 12 command queue.
	class command_queue {
		friend device;
		friend context;
	protected:
	private:
		_details::com_ptr<ID3D12CommandQueue> _queue; ///< The command queue.
	};

	/// DirectX 12 device implementation.
	class device {
		friend adapter;
		friend context;
	protected:
		/// Does not create a device.
		device(std::nullptr_t) {
		}

		/// Creates a \ref command_queue.
		[[nodiscard]] command_queue create_command_queue();
		/// Creates a \ref command_allocator.
		[[nodiscard]] command_allocator create_command_allocator();
		/*/// Creates a descriptor pool.
		[[nodiscard]] descriptor_pool create_descriptor_pool();*/
	private:
		_details::com_ptr<ID3D12Device8> _device; ///< Pointer to the device.
	};

	/// An adapter used for creating devices.
	class adapter {
		friend context;
	protected:
		/// Does not initialize \ref _adapter.
		adapter(std::nullptr_t) {
		}

		/// Calls \p D3D12CreateDevice().
		[[nodiscard]] device create_device();
		/// Returns the properties of this adapter.
		[[nodiscard]] adapter_properties get_properties() const;
	private:
		_details::com_ptr<IDXGIAdapter1> _adapter; ///< The adapter.
	};
}
