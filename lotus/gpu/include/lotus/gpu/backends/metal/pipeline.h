#pragma once

/// \file
/// Metal shader utilities and render pipelines.

#include <cstddef>

#include "details.h"

namespace lotus::gpu::backends::metal {
	class command_list;
	class device;


	// TODO
	class shader_reflection {
	protected:
		shader_reflection(std::nullptr_t) {
			// TODO
		}

		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(const char8_t*) const {
			// TODO
		}

		[[nodiscard]] std::uint32_t get_resource_binding_count() const {
			// TODO
		}

		[[nodiscard]] shader_resource_binding get_resource_binding_at_index(std::uint32_t) const {
			// TODO
		}

		[[nodiscard]] std::uint32_t get_render_target_count() const {
			// TODO
		}

		[[nodiscard]] cvec3s get_thread_group_size() const {
			// TODO
		}

		[[nodiscard]] bool is_valid() const {
			// TODO
		}
	};

	// TODO
	class shader_library_reflection {
	protected:
		shader_library_reflection(std::nullptr_t) {
			// TODO
		}

		template <typename Callback> void enumerate_shaders(Callback &&cb) const {
			// TODO
		}

		[[nodiscard]] shader_reflection find_shader(std::u8string_view entry, shader_stage) const {
			// TODO
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
		NS::SharedPtr<MTL::Library> _lib; ///< The Metal library.

		/// Initializes \ref _lib.
		explicit shader_binary(NS::SharedPtr<MTL::Library> lib) : _lib(std::move(lib)) {
		}

		/// Checks that \ref _lib contains only one function, and returns it.
		NS::SharedPtr<MTL::Function> _get_single_function() const;
	};

	// TODO
	class pipeline_resources {
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

	// TODO
	class compute_pipeline_state {
	protected:
		compute_pipeline_state(std::nullptr_t) {
			// TODO
		}
	};

	// TODO
	class raytracing_pipeline_state {
	protected:
		raytracing_pipeline_state(std::nullptr_t) {
			// TODO
		}
	};

	// TODO
	class shader_group_handle {
	protected:
		/// No initialization.
		shader_group_handle(uninitialized_t) {
		}

		[[nodiscard]] std::span<const std::byte> data() const {
			// TODO
		}
	};

	// TODO
	class timestamp_query_heap {
	protected:
		timestamp_query_heap(std::nullptr_t) {
			// TODO
		}

		[[nodiscard]] bool is_valid() const {
			// TODO
		}
	};
}
