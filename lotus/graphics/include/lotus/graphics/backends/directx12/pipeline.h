#pragma once

/// \file
/// Pipeline-related DirectX 12 classes.

#include <d3d12.h>

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class device;
	class command_list;
	class graphics_pipeline_state;
	class compute_pipeline_state;

	/// Contains a \p D3D12_SHADER_BYTECODE.
	class shader {
		friend device;
	protected:
		/// Creates an empty shader object.
		shader(std::nullptr_t) : _shader{} {
		}
	private:
		D3D12_SHADER_BYTECODE _shader; ///< The shader code.
	};

	/// A \p ID3D12RootSignature.
	class pipeline_resources {
		friend command_list;
		friend device;
		friend graphics_pipeline_state;
		friend compute_pipeline_state;
	protected:
	private:
		using _root_param_index = std::uint8_t; ///< Root parameter index.
		/// Indicates that there's no root parameter corresponding to this descriptor table.
		constexpr static _root_param_index _invalid_root_param = std::numeric_limits<_root_param_index>::max();
		/// Indices of descriptor table bindings.
		struct _root_param_indices {
			/// No initialization.
			_root_param_indices(uninitialized_t) {
			}
			/// Initializes all indices to \ref _invalid_root_param.
			_root_param_indices(std::nullptr_t) :
				resource_index(_invalid_root_param), sampler_index(_invalid_root_param) {
			}

			std::uint8_t resource_index; ///< Index of the shader resource root parameter.
			std::uint8_t sampler_index; ///< Index of the sampler root parameter.
		};

		_details::com_ptr<ID3D12RootSignature> _signature; ///< The \p ID3D12RootSignature object.
		// TODO allocator
		/// Root parameter indices of all descriptor tables.
		std::vector<_root_param_indices> _descriptor_table_binding;
	};

	/// A \p ID3D12PipelineState, a \p ID3D12RootSignature, and additional data.
	class graphics_pipeline_state {
		friend device;
		friend command_list;
	protected:
		/// Creates an empty state object.
		graphics_pipeline_state(std::nullptr_t) {
		}
	private:
		/// Root parameter indices of all descriptor tables.
		std::span<const pipeline_resources::_root_param_indices> _descriptor_table_binding;
		_details::com_ptr<ID3D12PipelineState> _pipeline; ///< The \p ID3D12PipelineState object.
		_details::com_ptr<ID3D12RootSignature> _root_signature; ///< The root signature.
		D3D_PRIMITIVE_TOPOLOGY _topology; ///< Primitive topology used by this pipeline.
	};

	/// A \p ID3D12PipelineState and a \p ID3D12RootSignature.
	class compute_pipeline_state {
		friend device;
		friend command_list;
	protected:
		/// Creates an empty state object.
		compute_pipeline_state(std::nullptr_t) {
		}
	private:
		/// Root parameter indices of all descriptor tables.
		std::span<const pipeline_resources::_root_param_indices> _descriptor_table_binding;
		_details::com_ptr<ID3D12PipelineState> _pipeline; ///< The \p ID3D12PipelineState object.
		_details::com_ptr<ID3D12RootSignature> _root_signature; ///< The root signature.
	};
}
