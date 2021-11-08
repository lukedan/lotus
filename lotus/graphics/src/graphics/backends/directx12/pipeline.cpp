#include "lotus/graphics/backends/directx12/pipeline.h"

/// \file
/// Pipeline.

namespace lotus::graphics::backends::directx12 {
	std::optional<shader_resource_binding> shader_reflection::find_resource_binding_by_name(
		const char8_t *name
	) const {
		D3D12_SHADER_INPUT_BIND_DESC desc = {};
		// TODO error handling
		_details::assert_dx(_reflection->GetResourceBindingDescByName(reinterpret_cast<const char*>(name), &desc));
		return _details::conversions::back_to_shader_resource_binding(desc);
	}
}
