#include "lotus/graphics/backends/directx12/context.h"

/// \file
/// Implementation of the DirectX 12 backend.

namespace lotus::graphics::backends::directx12 {
	context::context() {
		_details::assert_dx(CreateDXGIFactory1(IID_PPV_ARGS(&_dxgi_factory)));
		{ // enable debug layer
			_details::com_ptr<ID3D12Debug1> debug;
			_details::assert_dx(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
			debug->EnableDebugLayer();
		}
	}

	swap_chain context::create_swap_chain_for_window(
		system::platforms::windows::window &wnd, device&, command_queue &q,
		std::size_t num_frames, format format
	) {
		swap_chain result;

		// create swap chain itself
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.Width = 0;
		desc.Height = 0;
		desc.Format = _details::conversions::for_format(format);
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

		_details::com_ptr<IDXGISwapChain1> swap_chain;
		_details::assert_dx(_dxgi_factory->CreateSwapChainForHwnd(
			q._queue.Get(), wnd._hwnd, &desc, &fullscreen_desc, nullptr, &swap_chain
		));
		_details::assert_dx(swap_chain->QueryInterface(IID_PPV_ARGS(&result._swap_chain)));

		result._on_presented.resize(num_frames, nullptr);

		return result;
	}
}
