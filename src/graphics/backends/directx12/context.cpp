#include "lotus/graphics/backends/directx12/context.h"

/// \file
/// Implementation of the DirectX 12 backend.

namespace lotus::graphics::backends::directx12 {
	device adapter::create_device() {
		device result = nullptr;
		_details::assert_dx(D3D12CreateDevice(
			_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&result._device)
		));
		return result;
	}

	adapter_properties adapter::get_properties() const {
		adapter_properties result;
		DXGI_ADAPTER_DESC1 desc;
		_details::assert_dx(_adapter->GetDesc1(&desc));
		// TODO
		return result;
	}


	context::context() {
		_details::assert_dx(CreateDXGIFactory1(IID_PPV_ARGS(&_dxgi_factory)));
	}

	swap_chain context::create_swap_chain_for_window(
		system::platforms::windows::window &wnd, device &dev, command_queue &q,
		std::size_t num_frames, pixel_format format
	) {
		swap_chain result;

		// create swap chain itself
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.Width = 0;
		desc.Height = 0;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO
		desc.Stereo = false;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = static_cast<UINT>(num_frames);
		desc.Scaling = DXGI_SCALING_NONE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {};
		fullscreen_desc.RefreshRate.Numerator = 60;
		fullscreen_desc.RefreshRate.Denominator = 1;
		fullscreen_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		fullscreen_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		fullscreen_desc.Windowed = true;

		_details::assert_dx(_dxgi_factory->CreateSwapChainForHwnd(
			q._queue.Get(), wnd._hwnd, &desc, &fullscreen_desc, nullptr, &result._swap_chain
		));

		// create descriptor heap and render target views
		D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
		descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		descriptor_heap_desc.NumDescriptors = static_cast<UINT>(num_frames);
		descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		descriptor_heap_desc.NodeMask = 0;
		_details::assert_dx(dev._device->CreateDescriptorHeap(
			&descriptor_heap_desc, IID_PPV_ARGS(&result._descriptor_heap)
		));

		D3D12_CPU_DESCRIPTOR_HANDLE handle = result._descriptor_heap->GetCPUDescriptorHandleForHeapStart();
		UINT descriptor_offset = dev._device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (std::size_t i = 0; i < num_frames; ++i) {
			_details::com_ptr<ID3D12Resource> buffer;
			_details::assert_dx(result._swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&buffer)));
			dev._device->CreateRenderTargetView(buffer.Get(), nullptr, handle);
			handle.ptr += descriptor_offset;
		}

		return result;
	}
}
