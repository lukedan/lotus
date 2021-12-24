#pragma once

/// \file
/// DirectX 12 acceleration structures.

#include <vector>
#include <span>

#include "lotus/graphics/common.h"
#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class bottom_level_acceleration_structure;
	class command_list;
	class device;


	/// Contains an array of \p D3D12_RAYTRACING_GEOMETRY_DESC.
	class bottom_level_acceleration_structure_geometry {
		friend command_list;
		friend device;
	protected:
		/// Fills in the \p D3D12_RAYTRACING_GEOMETRY_DESC structures.
		[[nodiscard]] static bottom_level_acceleration_structure_geometry create(
			std::span<const std::pair<vertex_buffer_view, index_buffer_view>>
		);
	private:
		/// Ready-to-use structure pointing to \ref _geometries.
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS _inputs;
		// TODO allocator
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> _geometries; ///< The list of geometries.
	};

	/// Contains a \p D3D12_RAYTRACING_INSTANCE_DESC.
	class instance_description {
		friend bottom_level_acceleration_structure;
	protected:
		/// No initialization.
		instance_description(uninitialized_t) {
		}
	private:
		D3D12_RAYTRACING_INSTANCE_DESC _desc; ///< Description data.
	};

	/// Contains a buffer and an offset in the buffer to the acceleration structure.
	class bottom_level_acceleration_structure {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty acceleration structure.
		bottom_level_acceleration_structure(std::nullptr_t) {
		}

		/// Fills out the \p D3D12_RAYTRACING_INSTANCE_DESC structure.
		[[nodiscard]] instance_description get_description(
			mat44f, std::uint32_t id, std::uint8_t mask, std::uint32_t hit_group_offset
		) const;
	private:
		_details::com_ptr<ID3D12Resource> _buffer; ///< The buffer.
		std::size_t _offset; ///< Offset in bytes from the beginning of the buffer.
	};

	/// Contains a buffer and an offset in the buffer to the acceleration structure.
	class top_level_acceleration_structure {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty acceleration structure.
		top_level_acceleration_structure(std::nullptr_t) {
		}
	private:
		_details::com_ptr<ID3D12Resource> _buffer; ///< The buffer.
		std::size_t _offset; ///< Offset in bytes from the beginning of the buffer.
	};
}
