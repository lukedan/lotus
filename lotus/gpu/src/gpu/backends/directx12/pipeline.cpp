#include "lotus/gpu/backends/directx12/pipeline.h"

/// \file
/// Pipeline.

namespace lotus::gpu::backends::directx12 {
	std::optional<shader_resource_binding> shader_reflection::find_resource_binding_by_name(
		const char8_t *name
	) const {
		D3D12_SHADER_INPUT_BIND_DESC desc = {};
		HRESULT hr = _reflection->GetResourceBindingDescByName(reinterpret_cast<const char*>(name), &desc);
		if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
			return std::nullopt;
		}
		_details::assert_dx(hr);
		return _details::conversions::back_to_shader_resource_binding(desc);
	}

	std::size_t shader_reflection::get_output_variable_count() const {
		D3D12_SHADER_DESC desc = {};
		_details::assert_dx(_reflection->GetDesc(&desc));
		return desc.OutputParameters;
	}

	cvec3s shader_reflection::get_thread_group_size() const {
		UINT x, y, z;
		_reflection->GetThreadGroupSize(&x, &y, &z);
		return cvec3s(x, y, z);
	}
}
