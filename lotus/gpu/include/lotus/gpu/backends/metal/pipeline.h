#pragma once

/// \file
/// Metal shader utilities and render pipelines.

#include <cstddef>

#include "details.h"
#include "lotus/gpu/backends/common/dxil_reflection.h"

namespace lotus::gpu::backends::metal {
	class command_list;
	class device;
	class shader_library_reflection;
	class shader_utility;


	/// Metal uses DXIL reflection. The reason for this is that Metal shader reflection information cannot be
	/// retrieved from the bytecode directly; instead it has to be either retrieved from the pipeline or the compiled
	/// \p IRObject.
	class shader_reflection : private common::dxil_reflection {
		friend shader_library_reflection;
		friend shader_utility;
	protected:
		/// Initializes this object to empty.
		shader_reflection(std::nullptr_t) : dxil_reflection(nullptr) {
		}

		/// Calls the method in the base class.
		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(
			const char8_t *name
		) const {
			return dxil_reflection::find_resource_binding_by_name(name);
		}
		/// Calls the method in the base class.
		[[nodiscard]] u32 get_resource_binding_count() const {
			return dxil_reflection::get_resource_binding_count();
		}
		/// Calls the method in the base class.
		[[nodiscard]] shader_resource_binding get_resource_binding_at_index(u32 i) const {
			return dxil_reflection::get_resource_binding_at_index(i);
		}

		/// Calls the method in the base class.
		[[nodiscard]] u32 get_render_target_count() const {
			return dxil_reflection::get_render_target_count();
		}

		/// Calls the method in the base class.
		[[nodiscard]] cvec3u32 get_thread_group_size() const {
			return dxil_reflection::get_thread_group_size();
		}

		/// Calls the method in the base class.
		[[nodiscard]] bool is_valid() const {
			return dxil_reflection::is_valid();
		}
	private:
		/// Initializes the base class object.
		explicit shader_reflection(dxil_reflection reflection) : dxil_reflection(std::move(reflection)) {
		}
	};

	/// DXIL reflection for shader libraries.
	class shader_library_reflection : private common::dxil_library_reflection {
		friend shader_utility;
	protected:
		/// Initializes this object to empty.
		shader_library_reflection(std::nullptr_t) : dxil_library_reflection(nullptr) {
		}

		/// Calls the method in the base class.
		[[nodiscard]] u32 get_num_shaders() const {
			return dxil_library_reflection::get_num_shaders();
		}
		/// Calls the method in the base class.
		[[nodiscard]] shader_reflection get_shader_at(u32 i) const {
			return shader_reflection(dxil_library_reflection::get_shader_at(i));
		}
		/// Calls the method in the base class.
		[[nodiscard]] shader_reflection find_shader(std::u8string_view entry, shader_stage stage) const {
			return shader_reflection(dxil_library_reflection::find_shader(entry, stage));
		}
	private:
		/// Initializes the base class object.
		explicit shader_library_reflection(dxil_library_reflection refl) : dxil_library_reflection(std::move(refl)) {
		}
	};

	/// Holds a \p MTL::Library.
	class shader_binary {
		friend device;
	protected:
		/// Initializes this object to empty.
		shader_binary(std::nullptr_t) {
		}
	private:
		/// Describes a vertex input attribute.
		struct _vertex_input_attribute {
			std::u8string name; ///< The semantic of this attribute.
			u8 attribute_index; ///< The index of this attribute.
		};

		NS::SharedPtr<MTL::Library> _lib; ///< The Metal library.
		std::vector<_vertex_input_attribute> _vs_input_attributes; ///< Vertex shader input attributes.
		cvec3u32 _thread_group_size = zero; ///< Compute shader thread group size.
	};

	// TODO
	class pipeline_resources {
		friend device;
	protected:
		pipeline_resources(std::nullptr_t) {
			// TODO
		}
	};

	/// Contains a \p MTL::RenderPipelineState, a \p MTL::DepthStencilState, and a \ref rasterizer_options for the
	/// full state of the pipeline.
	class graphics_pipeline_state {
		friend command_list;
		friend device;
	protected:
		/// Initializes this object to empty.
		graphics_pipeline_state(std::nullptr_t) {
		}
	private:
		NS::SharedPtr<MTL::RenderPipelineState> _pipeline; ///< The pipeline state object.
		NS::SharedPtr<MTL::DepthStencilState> _ds_state; ///< The depth-stencil state object.
		rasterizer_options _rasterizer_options = nullptr; ///< Rasterizer options.
		primitive_topology _topology = primitive_topology::num_enumerators; ///< Topology.

		/// Initializes all fields of this struct.
		graphics_pipeline_state(
			NS::SharedPtr<MTL::RenderPipelineState> p,
			NS::SharedPtr<MTL::DepthStencilState> ds,
			rasterizer_options r,
			primitive_topology t
		) : _pipeline(std::move(p)), _ds_state(std::move(ds)), _rasterizer_options(r), _topology(t) {
		}
	};

	/// Contains a \p MTL::ComputePipelineState.
	class compute_pipeline_state {
		friend command_list;
		friend device;
	protected:
		/// Initializes the object to empty.
		compute_pipeline_state(std::nullptr_t) {
		}
	private:
		NS::SharedPtr<MTL::ComputePipelineState> _pipeline; ///< The pipeline state object.
		cvec3u32 _thread_group_size = zero;

		/// Initializes all fields of this struct.
		compute_pipeline_state(NS::SharedPtr<MTL::ComputePipelineState> p, cvec3u32 thread_group_size) :
			_pipeline(std::move(p)), _thread_group_size(thread_group_size) {
		}
	};

	// TODO
	class raytracing_pipeline_state {
	protected:
		raytracing_pipeline_state(std::nullptr_t) {
			// TODO
		}
	};

	/// Contains a \p IRShaderIdentifier.
	class shader_group_handle {
		friend device;
	protected:
		/// No initialization.
		shader_group_handle(uninitialized_t) {
		}

		/// Returns the raw contents of the \p IRShaderIdentifier.
		[[nodiscard]] std::span<const std::byte> data() const {
			static_assert(
				std::is_standard_layout_v<IRShaderIdentifier>,
				"IRShaderIdentifier should be a standard layout type"
			);
			return std::as_bytes(std::span(&_id, 1));
		}

		IRShaderIdentifier _id;
	};

	/// Contains a \p MTL4::CounterHeap.
	class timestamp_query_heap {
		friend command_list;
		friend device;
	protected:
		/// Initializes this object to empty.
		timestamp_query_heap(std::nullptr_t) {
		}

		/// Checks if this object is valid.
		[[nodiscard]] bool is_valid() const {
			return !!_heap;
		}
	private:
		NS::SharedPtr<MTL4::CounterHeap> _heap; ///< The counter heap.

		/// Initializes \ref _buf.
		explicit timestamp_query_heap(NS::SharedPtr<MTL4::CounterHeap> heap) : _heap(std::move(heap)) {
		}
	};
}
