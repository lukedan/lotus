#pragma once

/// \file
/// Implementation details and helpers for the Metal backend.

#include <cstddef>
#include <utility>
#include <map>

#include <Metal/Metal.hpp>

#include <metal_irconverter/metal_irconverter.h>

#include "lotus/utils/static_function.h"
#include "lotus/gpu/common.h"
#include "lotus/gpu/backends/common/dxil_reflection.h"

namespace lotus::gpu::backends::metal::_details {
	// TODO
	/// Metal does not seem to have custom debug callbacks.
	using debug_message_id = int;
	/// Debug callback type for the Metal backend.
	using debug_message_callback =
		static_function<void(debug_message_severity, debug_message_id, std::u8string_view)>;


	/// A pointer that has an associated residency set, from which the resource is removed upon disposal.
	template <typename T> struct residency_ptr {
	public:
		/// Initializes this object to empty.
		residency_ptr(std::nullptr_t) {
		}
		/// Initializes the pointer, and adds the resource to the residency set.
		residency_ptr(NS::SharedPtr<T> ptr, MTL::ResidencySet *set) : _ptr(std::move(ptr)), _residency_set(set) {
			if (_residency_set) {
				_residency_set->addAllocation(_ptr.get());
				_residency_set->commit(); // TODO committing immediately
			}
		}
		/// Move constructor.
		residency_ptr(residency_ptr &&src) :
			_ptr(std::move(src._ptr)),
			_residency_set(std::exchange(src._residency_set, nullptr)) {
		}
		/// No copy constructor.
		residency_ptr(const residency_ptr&) = delete;
		/// Move assignment.
		residency_ptr &operator=(residency_ptr &&src) {
			if (this != &src) {
				_remove_allocation();
				_ptr = std::move(src._ptr);
				_residency_set = std::exchange(src._residency_set, nullptr);
			}
			return *this;
		}
		/// No copy assignment.
		residency_ptr& operator=(const residency_ptr&) = delete;
		/// Removes the allocation from the residency set if necessary.
		~residency_ptr() {
			_remove_allocation();
		}

		/// Returns the underlying allocation.
		[[nodiscard]] T *get() const {
			return _ptr.get();
		}
		/// \overload
		[[nodiscard]] T *operator->() const {
			return _ptr.get();
		}
		/// Returns the shared pointer.
		[[nodiscard]] const NS::SharedPtr<T> &get_ptr() const {
			return _ptr;
		}

		/// Checks if this holds a valid object.
		[[nodiscard]] explicit operator bool() const {
			return !!_ptr;
		}
	private:
		NS::SharedPtr<T> _ptr; ///< The allocation.
		MTL::ResidencySet *_residency_set = nullptr; ///< The associated residency set.

		/// Removes the allocation from the residency set.
		void _remove_allocation() {
			if (_residency_set) {
				_residency_set->removeAllocation(_ptr.get());
				_residency_set->commit(); // TODO committing immediately
			}
		}
	};


	/// Memory types supported by Metal.
	enum class memory_type_index : std::underlying_type_t<gpu::memory_type_index> {
		shared_cpu_cached,   ///< \p MTL::StorageModeShared with CPU-side caching enabled.
		shared_cpu_uncached, ///< \p MTL::StorageModeShared with CPU-side caching disabled.
		device_private,      ///< \p MTL::StroageModePrivate.

		num_enumerators ///< The number of enumerators.
	};

	/// Conversion helpers between lotus and Metal data types.
	namespace conversions {
		/// Converts a \ref format to a \p MTL::PixelFormat.
		[[nodiscard]] MTL::PixelFormat to_pixel_format(format);
		/// Converts a \ref format to a \p MTL::VertexFormat.
		[[nodiscard]] MTL::VertexFormat to_vertex_format(format);
		/// Converts a \ref format to a \p MTL::AttributeFormat.
		[[nodiscard]] MTL::AttributeFormat to_attribute_format(format);
		/// Converts a \ref memory_type_index to a \p MTL::ResourceOptions.
		[[nodiscard]] MTL::ResourceOptions to_resource_options(memory_type_index);
		/// \overload
		[[nodiscard]] inline MTL::ResourceOptions to_resource_options(gpu::memory_type_index i) {
			return to_resource_options(static_cast<memory_type_index>(i));
		}
		/// Converts a \ref mip_levels to a \p NS::Range based on an associated texture.
		[[nodiscard]] NS::Range to_range(mip_levels, MTL::Texture*);
		/// Converts a \ref image_usage_mask to a \p MTL::TextureUsage.
		[[nodiscard]] MTL::TextureUsage to_texture_usage(image_usage_mask);
		/// Converts a \ref sampler_address_mode to a \p MTL::SamplerAddressMode.
		[[nodiscard]] MTL::SamplerAddressMode to_sampler_address_mode(sampler_address_mode);
		/// Converts a \ref filtering to a \p MTL::SamplerMinMagFilter.
		[[nodiscard]] MTL::SamplerMinMagFilter to_sampler_min_mag_filter(filtering);
		/// Converts a \ref filtering to a \p MTL::SamplerMipFilter.
		[[nodiscard]] MTL::SamplerMipFilter to_sampler_mip_filter(filtering);
		/// Converts a \ref comparison_function to a \p MTL::CompareFunction.
		[[nodiscard]] MTL::CompareFunction to_compare_function(comparison_function);
		/// Converts a \ref pass_load_operation to a \p MTL::LoadAction.
		[[nodiscard]] MTL::LoadAction to_load_action(pass_load_operation);
		/// Converts a \ref pass_store_operation to a \p MTL::StoreAction.
		[[nodiscard]] MTL::StoreAction to_store_action(pass_store_operation);
		/// Converts a \ref primitive_topology to a \p MTL::PrimitiveType.
		[[nodiscard]] MTL::PrimitiveType to_primitive_type(primitive_topology);
		/// Converts a \ref primitive_topology to a \p MTL::PrimitiveTopologyClass.
		[[nodiscard]] MTL::PrimitiveTopologyClass to_primitive_topology_class(primitive_topology);
		/// Converts a \ref index_format to a \p MTL::IndexType.
		[[nodiscard]] MTL::IndexType to_index_type(index_format);
		/// Converts a \ref input_buffer_rate to a \p MTL::VertexStepFunction.
		[[nodiscard]] MTL::VertexStepFunction to_vertex_step_function(input_buffer_rate);
		/// Converts a \ref front_facing_mode to a \p MTL::Winding.
		[[nodiscard]] MTL::Winding to_winding(front_facing_mode);
		/// Converts a \ref cull_mode to a \p MTL::CullMode.
		[[nodiscard]] MTL::CullMode to_cull_mode(cull_mode);
		/// Converts a \ref stencil_operation to a \p MTL::StencilOperation.
		[[nodiscard]] MTL::StencilOperation to_stencil_operation(stencil_operation);
		/// Converts a \ref blend_operation to a \p MTL::BlendOperation.
		[[nodiscard]] MTL::BlendOperation to_blend_operation(blend_operation);
		/// Converts a \ref blend_factor to a \p MTL::BlendFactor.
		[[nodiscard]] MTL::BlendFactor to_blend_factor(blend_factor);
		/// Converts a \ref channel_mask to a \p MTL::ColorWriteMask.
		[[nodiscard]] MTL::ColorWriteMask to_color_write_mask(channel_mask);
		/// Converts a \ref context_options to a \p MTL::ShaderValidation.
		[[nodiscard]] MTL::ShaderValidation to_shader_validation(context_options);
		/// Converts a \ref raytracing_instance_flags to a \p MTL::AccelerationStructureInstanceOptions.
		[[nodiscard]] MTL::AccelerationStructureInstanceOptions to_acceleration_structure_instance_options(
			raytracing_instance_flags
		);

		/// Converts a \ref D3D_SHADER_INPUT_TYPE to a \p IRDescriptorRangeType.
		[[nodiscard]] IRDescriptorRangeType to_ir_descriptor_range_type(D3D_SHADER_INPUT_TYPE);
		/// Converts a \ref shader_stage back to a \p IRShaderStage.
		[[nodiscard]] IRShaderStage to_ir_shader_stage(shader_stage);

		/// Converts a C-style string to a \p NS::String.
		[[nodiscard]] NS::SharedPtr<NS::String> to_string(const char8_t*);
		/// Converts a \ref stencil_options to a \p MTL::StencilDescriptor.
		[[nodiscard]] NS::SharedPtr<MTL::StencilDescriptor> to_stencil_descriptor(
			stencil_options, std::uint8_t stencil_read, std::uint8_t stencil_write
		);
		/// Converts a \ref cvec3 to a \p MTL::Size.
		[[nodiscard]] MTL::Size to_size(cvec3<NS::UInteger>);
		/// Converts a \ref mat34f to a \p MTL::PackedFloat4x3.
		[[nodiscard]] MTL::PackedFloat4x3 to_packed_float4x3(mat34f);

		/// Converts a \p NS::String back to a \p std::string.
		[[nodiscard]] std::u8string back_to_string(NS::String*);
		/// Converts a \p MTL::SizeAndAlign back to a \ref memory::size_alignment.
		[[nodiscard]] memory::size_alignment back_to_size_alignment(MTL::SizeAndAlign);
		/// Converts a \p MTL::AccelerationStructureSizes back to a \ref acceleration_structure_build_sizes.
		[[nodiscard]] acceleration_structure_build_sizes back_to_acceleration_structure_build_sizes(
			MTL::AccelerationStructureSizes
		);

		/// Converts a \p D3D12_SHADER_INPUT_BIND_DESC to a \p IRDescriptorRange1.
		[[nodiscard]] IRDescriptorRange1 d3d12_shader_input_bind_desc_to_ir_descriptor_range(
			D3D12_SHADER_INPUT_BIND_DESC
		);
	}

	/// Creates a new \p MTL::TextureDescriptor based on the given settings.
	[[nodiscard]] NS::SharedPtr<MTL::TextureDescriptor> create_texture_descriptor(
		MTL::TextureType,
		format,
		cvec3u32 size,
		std::uint32_t mip_levels,
		MTL::ResourceOptions,
		image_usage_mask
	);


	/// Checks whether the pixel format has a depth component.
	[[nodiscard]] bool does_pixel_format_have_depth(MTL::PixelFormat);
	/// Checks whether the pixel format has a stencil component.
	[[nodiscard]] bool does_pixel_format_have_stencil(MTL::PixelFormat);

	/// Retrieves the single shader function inside the shader library.
	[[nodiscard]] NS::SharedPtr<MTL::Function> get_single_shader_function(MTL::Library*);


	/// Declaration of deleter types for various IR converter library types.
	template <typename T> struct ir_object_deleter;
	/// Shorthand for declaring a deleter with a function.
	template <auto Func> struct basic_ir_object_deleter {
		/// Calls \p Func with the given object.
		void operator()(auto *obj) const {
			Func(obj);
		}
	};
	/// Calls \p IRObjectDestroy() for \p IRObject.
	template <> struct ir_object_deleter<IRObject> : basic_ir_object_deleter<IRObjectDestroy> {
	};
	/// Calls \p IRCompilerDestroy() for \p IRCompiler.
	template <> struct ir_object_deleter<IRCompiler> : basic_ir_object_deleter<IRCompilerDestroy> {
	};
	/// Calls \p IRRootSignatureDestroy() for \p IRRootSignature.
	template <> struct ir_object_deleter<IRRootSignature> : basic_ir_object_deleter<IRRootSignatureDestroy> {
	};
	/// Calls \p IRMetalLibBinaryDestroy() for \p IRMetalLibBinary.
	template <> struct ir_object_deleter<IRMetalLibBinary> : basic_ir_object_deleter<IRMetalLibBinaryDestroy> {
	};
	/// Calls \p IRShaderReflectionDestroy for \p IRShaderReflection.
	template <> struct ir_object_deleter<IRShaderReflection> : basic_ir_object_deleter<IRShaderReflectionDestroy> {
	};
	/// Unique pointer type for Metal shader converter types.
	template <typename T> using ir_unique_ptr = std::unique_ptr<T, ir_object_deleter<T>>;
	/// Creates a new \ref ir_unique_ptr.
	template <typename T> [[nodiscard]] static ir_unique_ptr<T> ir_make_unique(T *ptr) {
		return ir_unique_ptr<T>(ptr);
	}

	namespace shader {
		/// Contains all relevant IR conversion results.
		struct ir_conversion_result {
			/// Initializes this object to empty.
			ir_conversion_result(std::nullptr_t) {
			}

			ir_unique_ptr<IRObject> object; ///< Object containing the IR.
			dispatch_data_t data = nullptr; ///< Raw IR bytes.
		};

		/// Creates a \p IRRootSignature matching the given list of \p IRRootParameter1 objects.
		[[nodiscard]] ir_unique_ptr<IRRootSignature> create_root_signature_for_bindings(std::span<IRRootParameter1>);
		/// Creates a \p IRRootSignature matching the given \p ID3D12ShaderReflection.
		[[nodiscard]] ir_unique_ptr<IRRootSignature> create_root_signature_for_shader_reflection(
			ID3D12ShaderReflection*
		);
		/// Converts the given DXIL into Metal IR, with a root signature derived from the given reflection object.
		[[nodiscard]] ir_conversion_result convert_to_metal_ir(
			std::span<const std::byte> dxil, IRRootSignature*
		);
	}
}
