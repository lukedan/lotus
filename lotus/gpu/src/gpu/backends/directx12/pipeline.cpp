#include "lotus/gpu/backends/directx12/pipeline.h"

/// \file
/// Pipeline.

#include "lotus/utils/strings.h"

namespace lotus::gpu::backends::directx12 {
	std::optional<shader_resource_binding> shader_reflection::find_resource_binding_by_name(
		const char8_t *name
	) const {
		D3D12_SHADER_INPUT_BIND_DESC desc = {};
		HRESULT hr = std::visit([&](const auto &refl) {
			return refl->GetResourceBindingDescByName(reinterpret_cast<const char*>(name), &desc);
		}, _reflection);
		if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
			return std::nullopt;
		}
		_details::assert_dx(hr);
		return _details::conversions::back_to_shader_resource_binding(desc);
	}

	std::size_t shader_reflection::get_output_variable_count() const {
		if (std::holds_alternative<_shader_refl_ptr>(_reflection)) {
			D3D12_SHADER_DESC desc = {};
			_details::assert_dx(std::get<_shader_refl_ptr>(_reflection)->GetDesc(&desc));
			return desc.OutputParameters;
		}
		return 0; // function shaders don't have output variables... do they?
	}

	cvec3s shader_reflection::get_thread_group_size() const {
		if (std::holds_alternative<_shader_refl_ptr>(_reflection)) {
			UINT x, y, z;
			std::get<_shader_refl_ptr>(_reflection)->GetThreadGroupSize(&x, &y, &z);
			return cvec3s(x, y, z);
		}
		return zero; // there doesn't seem to be a way to bundle a compute shader into a library
	}

	UINT shader_reflection::_get_resource_binding_count() const {
		return _visit_desc([](const auto &desc) {
			return desc.BoundResources;
		});
	}


	shader_reflection shader_library_reflection::find_shader(std::u8string_view name, shader_stage stage) const {
		auto version = _details::conversions::to_shader_version_type(stage);
		D3D12_LIBRARY_DESC desc = {};
		_details::assert_dx(_reflection->GetDesc(&desc));
		for (UINT i = 0; i < desc.FunctionCount; ++i) {
			auto *func = _reflection->GetFunctionByIndex(static_cast<INT>(i));
			D3D12_FUNCTION_DESC func_desc = {};
			_details::assert_dx(func->GetDesc(&func_desc));
			if (func_desc.Version == version) {
				if (std::strlen(func_desc.Name) == name.size()) {
					if (std::strncmp(func_desc.Name, string::to_generic(name).data(), name.size()) == 0) {
						return shader_reflection(func);
					}
				}
			}
		}
		return nullptr;
	}
}
