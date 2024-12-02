#pragma once

/// \file
/// Pipeline-related DirectX 12 classes.

#include <optional>

#include <directx/d3d12shader.h>

#include "details.h"

namespace lotus::gpu::backends::directx12 {
	class device;
	class command_list;
	class compute_pipeline_state;
	class graphics_pipeline_state;
	class raytracing_pipeline_state;
	class shader_utility;
	class shader_library_reflection;

	/// Contains a \p ID3D12ShaderReflection or a \p ID3D12FunctionReflection.
	class shader_reflection {
		friend shader_utility;
		friend shader_library_reflection;
	protected:
		/// Initializes an empty reflection object.
		shader_reflection(std::nullptr_t) {
		}

		/// Returns the result of \p ID3D12ShaderReflection::GetResourceBindingDescByName() or
		/// \p ID3D12FunctionReflection::GetResourceBindingDescByName().
		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(const char8_t*) const;
		/// Enumerates over all resource bindings using \p ID3D12ShaderReflection::GetResourceBindingDesc().
		template <typename Callback> void enumerate_resource_bindings(Callback &&cb) const {
			UINT count = _get_resource_binding_count();
			for (UINT i = 0; i < count; ++i) {
				D3D12_SHADER_INPUT_BIND_DESC desc = {};
				std::visit([&](const auto &refl) {
					_details::assert_dx(refl->GetResourceBindingDesc(i, &desc));
				}, _reflection);
				if (!cb(_details::conversions::back_to_shader_resource_binding(desc))) {
					break;
				}
			}
		}

		/// Returns the number of output variables.
		[[nodiscard]] std::size_t get_output_variable_count() const;
		/// Enumerates over all output variables using \p ID3D12ShaderReflection::GetOutputParameterDesc().
		template <typename Callback> void enumerate_output_variables(Callback &&cb) const {
			std::size_t count = get_output_variable_count();
			if (std::holds_alternative<_shader_refl_ptr>(_reflection)) {
				const auto &refl = std::get<_shader_refl_ptr>(_reflection);
				for (UINT i = 0; i < count; ++i) {
					D3D12_SIGNATURE_PARAMETER_DESC desc = {};
					_details::assert_dx(refl->GetOutputParameterDesc(i, &desc));
					if (!cb(_details::conversions::back_to_shader_output_variable(desc))) {
						break;
					}
				}
			}
		}

		/// Returns the result of \p ID3D12ShaderReflection::GetThreadGroupSize().
		[[nodiscard]] cvec3s get_thread_group_size() const;

		/// Returns whether this holds a valid object.
		[[nodiscard]] bool is_valid() const {
			return std::visit([](const auto &refl) {
				return refl != nullptr;
			}, _reflection);
		}
	private:
		/// Shared pointer to a \p ID3D12ShaderReflection.
		using _shader_refl_ptr = _details::com_ptr<ID3D12ShaderReflection>;
		/// Raw pointer to a \p ID3D12FunctionReflection.
		using _function_refl_ptr = ID3D12FunctionReflection*;
		/// Union of possible shader reflection types.
		using _union = std::variant<_shader_refl_ptr, _function_refl_ptr>;

		_union _reflection; ///< Shader reflection object.

		/// Initializes this object with a shader reflection object.
		explicit shader_reflection(_shader_refl_ptr ptr) :
			_reflection(std::in_place_type<_shader_refl_ptr>, std::move(ptr)) {
		}
		/// Initializes this object with a function reflection object.
		explicit shader_reflection(_function_refl_ptr ptr) :
			_reflection(std::in_place_type<_function_refl_ptr>, ptr) {
		}

		/// Calls the given callback with either a \p D3D12_SHADER_DESC or a \p D3D12_FUNCTION_DESC, depending on
		/// which type is currently held by this reflection obejct.
		template <typename Cb> auto _visit_desc(Cb &&cb) const {
			if (std::holds_alternative<_shader_refl_ptr>(_reflection)) {
				D3D12_SHADER_DESC desc = {};
				_details::assert_dx(std::get<_shader_refl_ptr>(_reflection)->GetDesc(&desc));
				return cb(desc);
			} else {
				D3D12_FUNCTION_DESC desc = {};
				_details::assert_dx(std::get<_function_refl_ptr>(_reflection)->GetDesc(&desc));
				return cb(desc);
			}
		}
		/// Returns the number of resource bindings.
		[[nodiscard]] UINT _get_resource_binding_count() const;
	};

	/// Contains a \p ID3D12LibraryReflection.
	class shader_library_reflection {
		friend shader_utility;
	protected:
		/// Initializes an empty reflection object.
		shader_library_reflection(std::nullptr_t) {
		}

		/// Enumerates over all shaders using \p ID3D12LibraryReflection::GetFunctionByIndex().
		template <typename Callback> void enumerate_shaders(Callback &&cb) const {
			D3D12_LIBRARY_DESC desc = {};
			_details::assert_dx(_reflection->GetDesc(&desc));
			for (UINT i = 0; i < desc.FunctionCount; ++i) {
				auto *refl = _reflection->GetFunctionByIndex(static_cast<INT>(i));
				if (!cb(shader_reflection(refl))) {
					break;
				}
			}
		}
		/// Finds the shader that matches the given entry point and stage using \ref enumerate_shaders().
		[[nodiscard]] shader_reflection find_shader(std::u8string_view, shader_stage) const;
	private:
		_details::com_ptr<ID3D12LibraryReflection> _reflection; ///< Shader library reflection object.
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
		_details::com_ptr<ID3D12StateObjectProperties> _properties; ///< State object properties.
	};

	/// Contains a binary blob that identifies a shader group.
	class shader_group_handle {
		friend device;
	protected:
		/// No initialization.
		shader_group_handle(uninitialized_t) {
		}

		/// Returns the shader group handle data.
		[[nodiscard]] std::span<const std::byte> data() const {
			return _id;
		}
	private:
		std::array<std::byte, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES> _id; ///< Shader identifier data.
	};

	///	Holds a \p ID3D12QueryHeap, as well as a buffer for holding results.
	class timestamp_query_heap {
		friend device;
		friend command_list;
	protected:
		/// Initializes this heap to empty.
		timestamp_query_heap(std::nullptr_t) {
		}

		/// Tests if this object holds a valid heap.
		[[nodiscard]] bool is_valid() const {
			return _heap;
		}
	private:
		_details::com_ptr<ID3D12QueryHeap> _heap; ///< The query heap.
		_details::com_ptr<ID3D12Resource> _resource; ///< Buffer for holding results.
	};
}
