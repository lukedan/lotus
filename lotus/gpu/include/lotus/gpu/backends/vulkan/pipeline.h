#pragma once

/// \file
/// Pipelines.

#include <optional>

#include <vulkan/vulkan.hpp>
#include <spirv_reflect.h>

#include "details.h"
#include "lotus/memory/stack_allocator.h"

namespace lotus::gpu::backends::vulkan {
	class command_list;
	class device;
	class shader_utility;
	class shader_library_reflection;


	/// Contains a \p SpvReflectShaderModule with a specific entry point index.
	class shader_reflection {
		friend device;
		friend shader_utility;
		friend shader_library_reflection;
	protected:
		/// Creates an empty object.
		shader_reflection(std::nullptr_t) {
		}

		/// Iterates through the bindings and returns the one with the specified name.
		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(const char8_t*) const;
		/// Returns the number of bindings available to this entry point.
		[[nodiscard]] std::uint32_t get_resource_binding_count() const;
		/// Returns the resource binding at the given index.
		[[nodiscard]] shader_resource_binding get_resource_binding_at_index(std::uint32_t) const;

		/// Returns \p SpvReflectEntryPoint::output_variable_count.
		[[nodiscard]] std::uint32_t get_render_target_count() const;

		/// Returns the thread group size.
		[[nodiscard]] cvec3s get_thread_group_size() const;

		/// Returns whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return _reflection != nullptr;
		}
	private:
		// TODO *maybe* this is not needed for single-entry modules?
		/// Data cached for a specific entry point.
		struct _cached_data {
			/// Descriptor bindings used by the entry point.
			std::vector<shader_resource_binding> descriptor_bindings;
		};

		// TODO allocator
		std::shared_ptr<spv_reflect::ShaderModule> _reflection; ///< Reflection data.
		std::shared_ptr<_cached_data> _cache; ///< Additional cached data.
		std::uint32_t _entry_point_index = 0; ///< Entry point index of the relevant shader.

		/// Initializes all fields of this struct and precomputes the \ref _cached_data.
		shader_reflection(std::shared_ptr<spv_reflect::ShaderModule>, std::uint32_t entry_idx);
	};

	/// Contains a \p SpvReflectShaderModule.
	class shader_library_reflection {
		friend shader_utility;
	protected:
		/// Initializes this object to empty.
		shader_library_reflection(std::nullptr_t) {
		}

		/// Enumerates over all shaders in this shader library.
		template <typename Callback> void enumerate_shaders(Callback &cb) const {
			auto count = _reflection->GetEntryPointCount();
			for (std::uint32_t i = 0; i < count; ++i) {
				if (!cb(shader_reflection(_reflection, i))) {
					break;
				}
			}
		}
		///
		[[nodiscard]] shader_reflection find_shader(std::u8string_view entry, shader_stage) const;
	private:
		std::shared_ptr<spv_reflect::ShaderModule> _reflection; ///< Reflection data.
	};


	/// Contains a \p vk::UniqueShaderModule.
	class shader_binary {
		friend device;
	protected:
		/// Creates an empty object.
		shader_binary(std::nullptr_t) {
		}
	private:
		vk::UniqueShaderModule _module; ///< The shader module.
		spv_reflect::ShaderModule _reflection; ///< Reflection data.
	};

	/// Contains a \p vk::UniquePipelineLayout.
	class pipeline_resources {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty object.
		pipeline_resources(std::nullptr_t) {
		}
	private:
		vk::UniquePipelineLayout _layout; ///< The pipeline layout.
	};

	/// Contains a \p vk::UniquePipeline.
	class graphics_pipeline_state {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty object.
		graphics_pipeline_state(std::nullptr_t) {
		}
	private:
		vk::UniquePipeline _pipeline; ///< The pipeline state.
	};

	/// Contains a \p vk::UniquePipeline.
	class compute_pipeline_state {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty object.
		compute_pipeline_state(std::nullptr_t) {
		}
	private:
		vk::UniquePipeline _pipeline; ///< The pipeline state.
	};

	/// Contains a \p vk::UniquePipeline.
	class raytracing_pipeline_state {
		friend command_list;
		friend device;
	protected:
		/// Creates an empty object.
		raytracing_pipeline_state(std::nullptr_t) {
		}
	private:
		vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> _pipeline; ///< The pipeline state.
	};

	/// Contains a Vulkan shader group handle.
	class shader_group_handle {
		friend device;
	protected:
		/// No initialization.
		shader_group_handle(uninitialized_t) {
		}

		/// Returns \ref _data.
		[[nodiscard]] std::span<const std::byte> data() const {
			return _data;
		}
	private:
		// unfortunately, for vulkan this can be dynamic
		std::vector<std::byte> _data; ///< Shader group handle data.
	};

	/// Holds a \p vk::UniqueQueryPool.
	class timestamp_query_heap {
		friend command_list;
		friend device;
	protected:
		/// Initializes this heap to empty.
		timestamp_query_heap(std::nullptr_t) {
		}

		/// Tests if this object holds a valid query pool.
		[[nodiscard]] bool is_valid() const {
			return _pool.get();
		}
	private:
		vk::UniqueQueryPool _pool; ///< The query pool.
	};
}
