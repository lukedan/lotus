#pragma once

/// \file
/// Shader reflection implemented using DirectX 12's reflection classes.

#include "lotus/gpu/common.h"
#include "details.h"

namespace lotus::gpu::backends::common {
	/// Contains a \p ID3D12ShaderReflection or a \p ID3D12FunctionReflection.
	class dxil_reflection {
	public:
		/// Shared pointer to a \p ID3D12ShaderReflection.
		using shader_reflection_ptr = _details::com_ptr<ID3D12ShaderReflection>;
		/// Raw pointer to a \p ID3D12FunctionReflection.
		using function_reflection_ptr = ID3D12FunctionReflection*;
		/// Union of possible shader reflection types.
		using reflection_ptr_union = std::variant<shader_reflection_ptr, function_reflection_ptr>;

		/// Initializes an empty reflection object.
		dxil_reflection(std::nullptr_t) {
		}
		/// Initializes this object with a shader reflection object.
		explicit dxil_reflection(shader_reflection_ptr ptr) :
			_reflection(std::in_place_type<shader_reflection_ptr>, std::move(ptr)) {
		}
		/// Initializes this object with a function reflection object.
		explicit dxil_reflection(function_reflection_ptr ptr) :
			_reflection(std::in_place_type<function_reflection_ptr>, ptr) {
		}

		/// Returns the result of \p ID3D12ShaderReflection::GetResourceBindingDescByName() or
		/// \p ID3D12FunctionReflection::GetResourceBindingDescByName().
		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(const char8_t*) const;
		/// Returns the number of resource bindings.
		[[nodiscard]] std::uint32_t get_resource_binding_count() const;
		/// Returns the result of \p ID3D12ShaderReflection::GetResourceBindingDesc().
		[[nodiscard]] shader_resource_binding get_resource_binding_at_index(std::uint32_t) const;

		/// Returns the number of render targets.
		[[nodiscard]] std::uint32_t get_render_target_count() const;

		/// Returns the result of \p ID3D12ShaderReflection::GetThreadGroupSize().
		[[nodiscard]] cvec3u32 get_thread_group_size() const;

		/// Returns whether this holds a valid object.
		[[nodiscard]] bool is_valid() const {
			return std::visit([](const auto &refl) {
				return refl != nullptr;
			}, _reflection);
		}

		/// Returns the raw DirectX reflection object.
		[[nodiscard]] const reflection_ptr_union &get_raw_ptr() const {
			return _reflection;
		}
	private:
		reflection_ptr_union _reflection; ///< Shader reflection object.

		/// Calls the given callback with either a \p D3D12_SHADER_DESC or a \p D3D12_FUNCTION_DESC, depending on
		/// which type is currently held by this reflection obejct.
		template <typename Cb> auto _visit_desc(Cb &&cb) const {
			if (std::holds_alternative<shader_reflection_ptr>(_reflection)) {
				D3D12_SHADER_DESC desc = {};
				_details::assert_dx(std::get<shader_reflection_ptr>(_reflection)->GetDesc(&desc));
				return cb(desc);
			} else {
				D3D12_FUNCTION_DESC desc = {};
				_details::assert_dx(std::get<function_reflection_ptr>(_reflection)->GetDesc(&desc));
				return cb(desc);
			}
		}
	};

	/// Contains a \p ID3D12LibraryReflection.
	class dxil_library_reflection {
	public:
		/// Initializes an empty reflection object.
		dxil_library_reflection(std::nullptr_t) {
		}
		/// Initializes \ref _reflection.
		explicit dxil_library_reflection(_details::com_ptr<ID3D12LibraryReflection> r) : _reflection(std::move(r)) {
		}

		/// Retrieves the number of shaders from the \p D3D12_LIBRARY_DESC.
		[[nodiscard]] std::uint32_t get_num_shaders() const;
		/// Retrieves the corresponding shader using \p ID3D12LibraryReflection::GetFunctionByIndex().
		[[nodiscard]] dxil_reflection get_shader_at(std::uint32_t) const;
		/// Finds the shader that matches the given entry point and stage using \ref enumerate_shaders().
		[[nodiscard]] dxil_reflection find_shader(std::u8string_view, shader_stage) const;
	private:
		_details::com_ptr<ID3D12LibraryReflection> _reflection; ///< Shader library reflection object.
	};
}
