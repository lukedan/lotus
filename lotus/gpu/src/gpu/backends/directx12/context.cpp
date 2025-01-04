#include "lotus/gpu/backends/directx12/context.h"

/// \file
/// Implementation of the DirectX 12 backend.

#include <format>

#include <d3dcompiler.h>

#include "lotus/system/platforms/windows/details.h"
#include "lotus/memory/stack_allocator.h"

extern "C" {
	_declspec(dllexport) extern const uint32_t D3D12SDKVersion = 715;
	_declspec(dllexport) extern LPCSTR D3D12SDKPath = ".\\";
}

namespace lotus::gpu::backends::directx12 {
	context context::create(context_options opts, _details::debug_message_callback debug_cb) {
		context result;
		_details::assert_dx(CreateDXGIFactory1(IID_PPV_ARGS(&result._dxgi_factory)));
		if (bit_mask::contains<context_options::enable_validation>(opts)) { // enable debug layer
			_details::com_ptr<ID3D12Debug1> debug;
			_details::assert_dx(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
			debug->EnableDebugLayer();
			debug->SetEnableGPUBasedValidation(true);
			debug->SetEnableSynchronizedCommandQueueValidation(true);
		}
		result._debug_message_callback = std::make_unique<_details::debug_message_callback>(std::move(debug_cb));
		return result;
	}

	std::vector<adapter> context::get_all_adapters() const {
		std::vector<adapter> adapters;
		for (UINT i = 0; ; ++i) {
			adapter adap = nullptr;
			const HRESULT result = _dxgi_factory->EnumAdapterByGpuPreference(
				i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adap._adapter)
			);
			if (result == DXGI_ERROR_NOT_FOUND) {
				break;
			}
			adap._debug_callback = _debug_message_callback.get();
			adapters.emplace_back(adap);
		}
		return adapters;
	}

	std::pair<swap_chain, format> context::create_swap_chain_for_window(
		system::platforms::windows::window &wnd, device&, command_queue &q,
		std::size_t num_frames, std::span<const format> formats
	) {
		swap_chain result = nullptr;

		DXGI_FORMAT dx_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		format result_format = format::r8g8b8a8_unorm;
		for (auto fmt : formats) {
			// check if the format is available
			// https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/bb173064(v=vs.85)
			DXGI_FORMAT cur_fmt = _details::conversions::to_format(fmt);
			switch (cur_fmt) {
			case DXGI_FORMAT_R8G8B8A8_UNORM: [[fallthrough]];
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				dx_format = DXGI_FORMAT_R8G8B8A8_UNORM;
				break;
			case DXGI_FORMAT_B8G8R8A8_UNORM: [[fallthrough]];
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				dx_format = DXGI_FORMAT_B8G8R8A8_UNORM;
				break;
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
				dx_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				break;
			default:
				continue; // not supported
			}
			result_format = fmt;
			break;
		}

		// create swap chain itself
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.Width              = 0;
		desc.Height             = 0;
		desc.Format             = dx_format;
		desc.Stereo             = false;
		desc.SampleDesc.Count   = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount        = static_cast<UINT>(num_frames);
		desc.Scaling            = DXGI_SCALING_NONE;
		desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {};
		fullscreen_desc.RefreshRate.Numerator   = 60;
		fullscreen_desc.RefreshRate.Denominator = 1;
		fullscreen_desc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		fullscreen_desc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
		fullscreen_desc.Windowed                = true;

		_details::com_ptr<IDXGISwapChain1> swap_chain;
		_details::assert_dx(_dxgi_factory->CreateSwapChainForHwnd(
			q._queue.Get(), wnd._hwnd, &desc, &fullscreen_desc, nullptr, &swap_chain
		));
		_details::assert_dx(swap_chain->QueryInterface(IID_PPV_ARGS(&result._swap_chain)));

		result._synchronization.resize(num_frames, nullptr);

		return { result, result_format };
	}


	shader_utility shader_utility::create() {
		return shader_utility();
	}

	/// Additional arguments supplied to the compiler.
	static const LPCWSTR _additional_args[] = { L"-Ges", L"-Zi", L"-Zpr" };
	shader_utility::compilation_result shader_utility::compile_shader(
		std::span<const std::byte> code_utf8, shader_stage stage, std::u8string_view entry,
		const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		return _compiler.compile_shader(
			code_utf8, stage, entry, shader_path, include_paths, defines, _additional_args
		);
	}

	shader_utility::compilation_result shader_utility::compile_shader_library(
		std::span<const std::byte> code_utf8,
		const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		return _compiler.compile_shader_library(code_utf8, shader_path, include_paths, defines, _additional_args);
	}

	shader_reflection shader_utility::load_shader_reflection(std::span<const std::byte> data) {
		shader_reflection::_shader_refl_ptr result = nullptr;
		_compiler.load_shader_reflection(data, IID_PPV_ARGS(result));
		return shader_reflection(std::move(result));
	}

	shader_library_reflection shader_utility::load_shader_library_reflection(std::span<const std::byte> data) {
		shader_library_reflection result = nullptr;
		_compiler.load_shader_reflection(data, IID_PPV_ARGS(result._reflection));
		return result;
	}
}
