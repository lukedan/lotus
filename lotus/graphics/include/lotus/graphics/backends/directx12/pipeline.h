#pragma once

/// \file
/// Pipeline-related DirectX 12 classes.

#include <optional>

#include <directx/d3d12shader.h>

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class device;
	class command_list;
	class compute_pipeline_state;
	class graphics_pipeline_state;
	class raytracing_pipeline_state;
	class shader_utility;

	/// Contains a \p ID3D12ShaderReflection
	class shader_reflection {
		friend shader_utility;
	protected:
		/// Initializes an empty reflection object.
		shader_reflection(std::nullptr_t) {
		}

		/// Returns the result of ID3D12ShaderReflection::GetResourceBindingDescByName().
		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(const char8_t*) const;
		/// Enumerates over all resource bindings using \p ID3D12ShaderReflection::GetResourceBindingDesc().
		template <typename Callback> void enumerate_resource_bindings(Callback &&cb) {
			D3D12_SHADER_DESC shader_desc = {};
			_details::assert_dx(_reflection->GetDesc(&shader_desc));
			for (UINT i = 0; i < shader_desc.BoundResources; ++i) {
				D3D12_SHADER_INPUT_BIND_DESC desc = {};
				_details::assert_dx(_reflection->GetResourceBindingDesc(i, &desc));
				if (!cb(_details::conversions::back_to_shader_resource_binding(desc))) {
					break;
				}
			}
		}
		/// Returns the number of output variables.
		[[nodiscard]] std::size_t get_output_variable_count();
		/// Enumerates over all output variables using \p ID3D12ShaderReflection::GetOutputParameterDesc().
		template <typename Callback> void enumerate_output_variables(Callback &&cb) {
			std::size_t count = get_output_variable_count();
			for (UINT i = 0; i < count; ++i) {
				D3D12_SIGNATURE_PARAMETER_DESC desc = {};
				_details::assert_dx(_reflection->GetOutputParameterDesc(i, &desc));
				if (!cb(_details::conversions::back_to_shader_output_variable(desc))) {
					break;
				}
			}
		}
	private:
		_details::com_ptr<ID3D12ShaderReflection> _reflection; ///< Shader reflection object.
	};

	/// Contains a \p D3D12_SHADER_BYTECODE.
	class shader_binary {
		friend device;
	protected:
		/// Creates an empty object.
		shader_binary(std::nullptr_t) : _shader{} {
		}
	private:
		// TODO allocator
		std::vector<std::byte> _code; ///< Shader code.
		D3D12_SHADER_BYTECODE _shader; ///< The shader code.
	};

	/// A \p ID3D12RootSignature.
	class pipeline_resources {
		friend command_list;
		friend device;
		friend graphics_pipeline_state;
		friend compute_pipeline_state;
		friend raytracing_pipeline_state;
	protected:
		/// Creates an empty object.
		pipeline_resources(std::nullptr_t) {
		}
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
		_details::com_ptr<ID3D12RootSignature> _root_signature; ///< The root signature.
		_details::com_ptr<ID3D12PipelineState> _pipeline; ///< The \p ID3D12PipelineState object.
	};

	/// Contains a \p ID3D12RootSignature and a \p ID3D12StateObject.
	class raytracing_pipeline_state {
		friend device;
		friend command_list;
	protected:
		/// Creates an empty state object.
		raytracing_pipeline_state(std::nullptr_t) {
		}
	private:
		/// Root parameter indices of all descriptor tables.
		std::span<const pipeline_resources::_root_param_indices> _descriptor_table_binding;
		_details::com_ptr<ID3D12RootSignature> _root_signature; ///< The root signature.
		_details::com_ptr<ID3D12StateObject> _state; ///< The \p ID3D12PipelineState object.
		_details::com_ptr<ID3D12StateObjectProperties> _properties; ///< Lazily-initialized state object properties.
	};

	/// Contains a \p void* that identifies a shader group.
	class shader_group_handle {
		friend device;
	protected:
		/// No initialization.
		shader_group_handle(uninitialized_t) {
		}
	private:
		std::array<std::byte, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES> _id; ///< Shader identifier data.
	};
}
