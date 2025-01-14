#include "lotus/gpu/backends/common/dxil_reflection.h"

/// \file
/// Implementation of DXIL reflection.

namespace lotus::gpu::backends::common {
	std::optional<shader_resource_binding> dxil_reflection::find_resource_binding_by_name(
		const char8_t *name
	) const {
		D3D12_SHADER_INPUT_BIND_DESC desc = {};
		const HRESULT hr = std::visit([&](const auto &refl) {
			return refl->GetResourceBindingDescByName(reinterpret_cast<const char*>(name), &desc);
		}, _reflection);
		// the HRESULT_FROM_WIN32 macro uses a ?: operator without wrapping it with parenthesis
		constexpr static HRESULT not_found = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		if (hr == not_found) {
			return std::nullopt;
		}
		_details::assert_dx(hr);
		return _details::conversions::back_to_shader_resource_binding(desc);
	}

	std::uint32_t dxil_reflection::get_resource_binding_count() const {
		return _visit_desc([](const auto &desc) {
			return desc.BoundResources;
		});
	}

	shader_resource_binding dxil_reflection::get_resource_binding_at_index(std::uint32_t i) const {
		D3D12_SHADER_INPUT_BIND_DESC desc = {};
		std::visit(
			[&](const auto &refl) {
				_details::assert_dx(refl->GetResourceBindingDesc(i, &desc));
			},
			_reflection
		);
		return _details::conversions::back_to_shader_resource_binding(desc);
	}

	std::uint32_t dxil_reflection::get_render_target_count() const {
		if (std::holds_alternative<shader_reflection_ptr>(_reflection)) {
			D3D12_SHADER_DESC desc = {};
			_details::assert_dx(std::get<shader_reflection_ptr>(_reflection)->GetDesc(&desc));
			return desc.OutputParameters;
		}
		return 0; // function shaders don't have output variables... do they?
	}

	cvec3s dxil_reflection::get_thread_group_size() const {
		if (std::holds_alternative<shader_reflection_ptr>(_reflection)) {
			UINT x, y, z;
			std::get<shader_reflection_ptr>(_reflection)->GetThreadGroupSize(&x, &y, &z);
			return cvec3s(x, y, z);
		}
		return zero; // there doesn't seem to be a way to bundle a compute shader into a library
	}


	std::uint32_t dxil_library_reflection::get_num_shaders() const {
		D3D12_LIBRARY_DESC desc = {};
		_details::assert_dx(_reflection->GetDesc(&desc));
		return desc.FunctionCount;
	}

	dxil_reflection dxil_library_reflection::get_shader_at(std::uint32_t i) const {
		return dxil_reflection(_reflection->GetFunctionByIndex(static_cast<INT>(i)));
	}

	dxil_reflection dxil_library_reflection::find_shader(std::u8string_view name, shader_stage stage) const {
		auto version = static_cast<UINT>(_details::conversions::to_shader_version_type(stage));
		D3D12_LIBRARY_DESC desc = {};
		_details::assert_dx(_reflection->GetDesc(&desc));
		for (UINT i = 0; i < desc.FunctionCount; ++i) {
			auto *func = _reflection->GetFunctionByIndex(static_cast<INT>(i));
			D3D12_FUNCTION_DESC func_desc = {};
			_details::assert_dx(func->GetDesc(&func_desc));
			if (func_desc.Version == version) {
				if (std::strlen(func_desc.Name) == name.size()) {
					if (std::strncmp(func_desc.Name, string::to_generic(name).data(), name.size()) == 0) {
						return dxil_reflection(func);
					}
				}
			}
		}
		return nullptr;
	}
}
