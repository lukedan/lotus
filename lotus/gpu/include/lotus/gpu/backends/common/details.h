#pragma once

/// \file
/// Common cross-platform GPU definitions.

#if _WIN32
#	include <winerror.h>
#	include <atlbase.h>
#	include <Unknwn.h>
#endif
#include LOTUS_GPU_DXC_HEADER

#if !_WIN32
// hack to make DirectX-Headers compile on MacOS
// https://github.com/microsoft/DirectX-Headers/issues/156
#	define LOTUS_METAL_DXC
#	pragma push_macro("interface")
#	define interface struct
#endif
#include <directx/d3d12shader.h>
#if !_WIN32
#	pragma pop_macro("interface")
#endif

#include "lotus/logging.h"
#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::common::_details {
	/// COM pointers.
	template <typename T> using com_ptr = CComPtr<T>;

	/// Checks that the given \p HRESULT indicates success.
	inline void assert_dx(HRESULT hr) {
		if (hr != S_OK) {
			log().error("DirectX error {}", hr);
			std::abort();
		}
	}

	namespace conversions {
		/// Converts a \ref shader_stage to a \p D3D12_SHADER_VERSION_TYPE.
		[[nodiscard]] D3D12_SHADER_VERSION_TYPE to_shader_version_type(shader_stage);

		/// Converts a \p D3D12_SHADER_INPUT_BIND_DESC back to a \ref shader_resource_binding.
		[[nodiscard]] shader_resource_binding back_to_shader_resource_binding(const D3D12_SHADER_INPUT_BIND_DESC&);
	}
}
