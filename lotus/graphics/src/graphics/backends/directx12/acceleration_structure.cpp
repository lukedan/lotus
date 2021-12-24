#include "lotus/graphics/backends/directx12/acceleration_structure.h"

/// \file
/// Implementation of DirectX 12 acceleration structures.

#include "lotus/graphics/resources.h"

namespace lotus::graphics::backends::directx12 {
	bottom_level_acceleration_structure_geometry bottom_level_acceleration_structure_geometry::create(
		std::span<const std::pair<vertex_buffer_view, index_buffer_view>> data
	) {
		bottom_level_acceleration_structure_geometry result;

		result._geometries.reserve(data.size());
		for (const auto &[vert, idx] : data) {
			auto &desc = result._geometries.emplace_back();
			desc.Type                                 = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			desc.Flags                                = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			desc.Triangles.Transform3x4               = 0;
			desc.Triangles.VertexFormat               = _details::conversions::for_format(vert.vertex_format);
			desc.Triangles.VertexCount                = static_cast<UINT>(vert.count);
			desc.Triangles.VertexBuffer.StartAddress  = vert.data->_buffer->GetGPUVirtualAddress() + vert.offset;
			desc.Triangles.VertexBuffer.StrideInBytes = vert.stride;
			if (idx.data) {
				desc.Triangles.IndexFormat = _details::conversions::for_index_format(idx.element_format);
				desc.Triangles.IndexCount  = static_cast<UINT>(idx.count);
				desc.Triangles.IndexBuffer = idx.data->_buffer->GetGPUVirtualAddress() + idx.offset;
			} else {
				desc.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
				desc.Triangles.IndexCount  = 0;
				desc.Triangles.IndexBuffer = 0;
			}
		}

		result._inputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		result._inputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		result._inputs.NumDescs       = static_cast<UINT>(result._geometries.size());
		result._inputs.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;
		result._inputs.pGeometryDescs = result._geometries.data();

		return result;
	}


	instance_description bottom_level_acceleration_structure::get_description(
		mat44f trans, std::uint32_t id, std::uint8_t mask, std::uint32_t hit_group_offset
	) const {
		instance_description result = uninitialized;
		for (std::size_t row = 0; row < 3; ++row) {
			for (std::size_t col = 0; col < 4; ++col) {
				result._desc.Transform[row][col] = trans(row, col);
			}
		}
		result._desc.InstanceID                          = id;
		result._desc.InstanceMask                        = mask;
		result._desc.InstanceContributionToHitGroupIndex = hit_group_offset;
		result._desc.Flags                               = D3D12_RAYTRACING_INSTANCE_FLAG_NONE; // TODO
		result._desc.AccelerationStructure               = _buffer->GetGPUVirtualAddress() + _offset;
		return result;
	}
}
