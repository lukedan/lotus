#include "lotus/graphics/backends/directx12/context.h"

/// \file
/// Implementation of the DirectX 12 backend.

#include <d3dcompiler.h>

#include "lotus/system/platforms/windows/details.h"
#include "lotus/utils/stack_allocator.h"

namespace lotus::graphics::backends::directx12 {
	context context::create() {
		context result;
		_details::assert_dx(CreateDXGIFactory1(IID_PPV_ARGS(&result._dxgi_factory)));
		{ // enable debug layer
			_details::com_ptr<ID3D12Debug1> debug;
			_details::assert_dx(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
			debug->EnableDebugLayer();
			debug->SetEnableGPUBasedValidation(true);
			debug->SetEnableSynchronizedCommandQueueValidation(true);
		}
		{ // allow unsigned shaders to run
			const IID features[] = { D3D12ExperimentalShaderModels };
			void *structs[] = { nullptr };
			UINT sizes[] = { 0 };
			_details::assert_dx(D3D12EnableExperimentalFeatures(1, features, structs, sizes));
		}
		return result;
	}

	std::pair<swap_chain, format> context::create_swap_chain_for_window(
		system::platforms::windows::window &wnd, device &dev, command_queue &q,
		std::size_t num_frames, std::span<const format> formats
	) {
		swap_chain result = nullptr;

		DXGI_FORMAT dx_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		format result_format = format::r8g8b8a8_unorm;
		for (auto fmt : formats) {
			// check if the format is available
			// https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/bb173064(v=vs.85)
			DXGI_FORMAT cur_fmt = _details::conversions::for_format(fmt);
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
		desc.Width = 0;
		desc.Height = 0;
		desc.Format = dx_format;
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

		result._synchronization.resize(num_frames, nullptr);

		return { result, result_format };
	}


	bool shader_utility::compilation_result::succeeded() const {
		HRESULT stat;
		_details::assert_dx(_result->GetStatus(&stat));
		return stat == S_OK;
	}

	std::u8string_view shader_utility::compilation_result::get_compiler_output() {
		if (!_messages) {
			_details::assert_dx(_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&_messages), nullptr));
		}
		return std::u8string_view(
			static_cast<const char8_t*>(_messages->GetBufferPointer()), _messages->GetBufferSize()
		);
	}

	std::span<const std::byte> shader_utility::compilation_result::get_compiled_binary() {
		if (!_binary) {
			_details::assert_dx(_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&_binary), nullptr));
		}
		return std::span(static_cast<const std::byte*>(_binary->GetBufferPointer()), _binary->GetBufferSize());
	}
	

	shader_utility shader_utility::create() {
		return shader_utility();
	}

	shader_reflection shader_utility::load_shader_reflection(std::span<const std::byte> data) {
		// create container reflection
		_details::com_ptr<IDxcBlob> blob;
		{
			_details::com_ptr<ID3DBlob> d3d_blob;
			_details::assert_dx(D3DCreateBlob(data.size(), &d3d_blob));
			_details::assert_dx(d3d_blob->QueryInterface(IID_PPV_ARGS(&blob)));
			std::memcpy(blob->GetBufferPointer(), data.data(), data.size());
		}
		_details::com_ptr<IDxcContainerReflection> container_reflection;
		_details::assert_dx(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container_reflection)));
		_details::assert_dx(container_reflection->Load(blob.Get()));

		UINT32 part_index;
		_details::com_ptr<IDxcBlob> reflection_blob;
		_details::assert_dx(container_reflection->FindFirstPartKind(DXC_PART_REFLECTION_DATA, &part_index));
		_details::assert_dx(container_reflection->GetPartContent(part_index, &reflection_blob));

		shader_reflection result = nullptr;
		DxcBuffer buf;
		buf.Encoding = DXC_CP_ACP;
		buf.Ptr      = reflection_blob->GetBufferPointer();
		buf.Size     = reflection_blob->GetBufferSize();
		_details::assert_dx(_utils().CreateReflection(&buf, IID_PPV_ARGS(&result._reflection)));
		return result;
	}

	shader_reflection shader_utility::load_shader_reflection(compilation_result &compiled) {
		shader_reflection result = nullptr;
		_details::com_ptr<IDxcBlob> refl;
		_details::assert_dx(compiled._result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&refl), nullptr));
		DxcBuffer buffer;
		buffer.Encoding = DXC_CP_ACP;
		buffer.Ptr      = refl->GetBufferPointer();
		buffer.Size     = refl->GetBufferSize();
		_details::assert_dx(_utils().CreateReflection(&buffer, IID_PPV_ARGS(&result._reflection)));
		return result;
	}

	shader_utility::compilation_result shader_utility::compile_shader(
		std::span<const std::byte> code, shader_stage stage, std::u8string_view entry
	) {
		constexpr static enum_mapping<shader_stage, std::wstring_view> stage_names{
			std::pair(shader_stage::all,             L"INVALID"),
			std::pair(shader_stage::vertex_shader,   L"vs"),
			std::pair(shader_stage::geometry_shader, L"gs"),
			std::pair(shader_stage::pixel_shader,    L"ps"),
			std::pair(shader_stage::compute_shader,  L"cs"),
		};

		auto bookmark = stack_allocator::for_this_thread().bookmark();

		auto entry_wstr = system::platforms::windows::_details::u8string_to_wstring(
			entry, bookmark.create_std_allocator<wchar_t>()
		);

		wchar_t profile[10];
		auto fmt_result = std::format_to_n(profile, std::size(profile) - 1, L"{}_{}_{}", stage_names[stage], 6, 0);
		assert(fmt_result.size + 1 < std::size(profile));
		profile[fmt_result.size] = L'\0';

		std::vector<LPCWSTR> args{
			L"-E", entry_wstr.c_str(),
			L"-T", profile,
			L"-Ges",
			L"-Zi",
			L"-Zpr",
		};

		DxcBuffer buffer;
		buffer.Ptr      = code.data();
		buffer.Size     = code.size();
		buffer.Encoding = DXC_CP_UTF8;
		compilation_result result;
		_details::assert_dx(_compiler().Compile(
			&buffer, args.data(), args.size(), nullptr, IID_PPV_ARGS(&result._result)
		));
		return result;
	}

	IDxcUtils &shader_utility::_utils() {
		if (!_dxc_utils) {
			_details::assert_dx(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&_dxc_utils)));
		}
		return *_dxc_utils.Get();
	}

	IDxcCompiler3 &shader_utility::_compiler() {
		if (!_dxc_compiler) {
			_details::assert_dx(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&_dxc_compiler)));
		}
		return *_dxc_compiler.Get();
	}
}
