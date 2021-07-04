#include "lotus/graphics/backends/directx12/device.h"

/// \file
/// Implementation of DirectX 12 devices.

namespace lotus::graphics::backends::directx12 {
	command_queue device::create_command_queue() {
		command_queue result;

		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		_details::assert_dx(_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&result._queue)));

		return result;
	}

	command_allocator device::create_command_allocator() {
		command_allocator result;

		_details::assert_dx(_device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&result._allocator)
		));

		return result;
	}

	/*descriptor_pool device::create_descriptor_pool() {
		descriptor_pool result;
		_device->CreateDescriptorHeap();
		return result;
	}*/
}
