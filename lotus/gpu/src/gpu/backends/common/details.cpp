#include "lotus/gpu/backends/common/details.h"

/// \file
/// Implementation of miscellaneous backend-agnostic functions.

namespace lotus::gpu::backends::common::_details::conversions {
	D3D12_SHADER_VERSION_TYPE to_shader_version_type(shader_stage stage) {
		constexpr static enums::sequential_mapping<shader_stage, D3D12_SHADER_VERSION_TYPE> table{
			std::pair(shader_stage::all,                   D3D12_SHVER_LIBRARY              ),
			std::pair(shader_stage::vertex_shader,         D3D12_SHVER_VERTEX_SHADER        ),
			std::pair(shader_stage::geometry_shader,       D3D12_SHVER_GEOMETRY_SHADER      ),
			std::pair(shader_stage::pixel_shader,          D3D12_SHVER_PIXEL_SHADER         ),
			std::pair(shader_stage::compute_shader,        D3D12_SHVER_COMPUTE_SHADER       ),
			std::pair(shader_stage::callable_shader,       D3D12_SHVER_CALLABLE_SHADER      ),
			std::pair(shader_stage::ray_generation_shader, D3D12_SHVER_RAY_GENERATION_SHADER),
			std::pair(shader_stage::intersection_shader,   D3D12_SHVER_INTERSECTION_SHADER  ),
			std::pair(shader_stage::any_hit_shader,        D3D12_SHVER_ANY_HIT_SHADER       ),
			std::pair(shader_stage::closest_hit_shader,    D3D12_SHVER_CLOSEST_HIT_SHADER   ),
			std::pair(shader_stage::miss_shader,           D3D12_SHVER_MISS_SHADER          ),
		};
		return table[stage];
	}

	shader_resource_binding back_to_shader_resource_binding(const D3D12_SHADER_INPUT_BIND_DESC &desc) {
		shader_resource_binding result = uninitialized;
		result.first_register = desc.BindPoint;
		result.register_count =
			desc.BindCount == 0 ?
			gpu::descriptor_range::unbounded_count :
			desc.BindCount;
		result.register_space = desc.Space;
		result.name           = reinterpret_cast<const char8_t*>(desc.Name);

		switch (desc.Type) {
		case D3D_SIT_CBUFFER:
			result.type = descriptor_type::constant_buffer;
			break;
		case D3D_SIT_TBUFFER:
			result.type = descriptor_type::read_only_buffer;
			break;
		case D3D_SIT_TEXTURE:
			result.type = descriptor_type::read_only_image;
			break;
		case D3D_SIT_UAV_RWTYPED:
			if (desc.Dimension == D3D_SRV_DIMENSION_BUFFER || desc.Dimension == D3D_SRV_DIMENSION_BUFFEREX) {
				result.type = descriptor_type::read_write_buffer;
			} else {
				result.type = descriptor_type::read_write_image;
			}
			break;
		case D3D_SIT_SAMPLER:
			result.type = descriptor_type::sampler;
			break;
		case D3D_SIT_STRUCTURED:
			result.type = descriptor_type::read_only_buffer;
			break;
		case D3D_SIT_UAV_RWSTRUCTURED:
			result.type = descriptor_type::read_write_buffer;
			break;
		case D3D_SIT_RTACCELERATIONSTRUCTURE:
			result.type = descriptor_type::acceleration_structure;
			break;
		default:
			crash_if(true); // not supported
			break;
		}

		return result;
	}
}
