#pragma once

/// \file
/// Pipelines.

#include <optional>

#include <vulkan/vulkan.hpp>
#include <spirv_reflect.h>

#include "details.h"
#include "lotus/utils/stack_allocator.h"

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
		/// Gets all \p SpvReflectDescriptorBinding using \p spv_reflect::ShaderModule::EnumerateDescriptorBindings()
		/// and then enumerates over them.
		template <typename Callback> void enumerate_resource_bindings(Callback &&cb) const {
			const auto &module = _reflection->GetShaderModule();
			const auto &entry = module.entry_points[_entry_point_index];
			auto past_last_uniform = entry.used_uniforms + entry.used_uniform_count;
			for (std::uint32_t i = 0; i < module.descriptor_binding_count; ++i) {
				const auto it = std::lower_bound(
					entry.used_uniforms,
					past_last_uniform,
					module.descriptor_bindings[i].spirv_id
				);
				if (it != past_last_uniform) {
					if (!cb(_details::conversions::back_to_shader_resource_binding(module.descriptor_bindings[i]))) {
						break;
					}
				}
			}
		}

		/// Queries the number of output variables using \p spv_reflect::ShaderModuleEnumerateOutputVariables().
		[[nodiscard]] std::size_t get_output_variable_count() const;
		/// Enumerates over all output varibles using \p spv_reflect::ShaderModuleEnumerateOutputVariables().
		template <typename Callback> void enumerate_output_variables(Callback &&cb) const {
			const auto &entry = _reflection->GetShaderModule().entry_points[_entry_point_index];
			auto count = entry.output_variable_count;
			for (std::uint32_t i = 0; i < count; ++i) {
				if (!cb(_details::conversions::back_to_shader_output_variable(*entry.output_variables[i]))) {
					break;
				}
			}
		}

		/// Returns the thread group size.
		[[nodiscard]] cvec3s get_thread_group_size() const;

		/// Returns whether this object is valid.
		[[nodiscard]] bool is_valid() const {
			return _reflection != nullptr;
		}
	private:
		// TODO allocator
		std::shared_ptr<spv_reflect::ShaderModule> _reflection; ///< Reflection data.
		std::uint32_t _entry_point_index = 0; ///< Entry point index of the relevant shader.

		/// Initializes all fields of this struct.
		shader_reflection(std::shared_ptr<spv_reflect::ShaderModule> ptr, std::uint32_t entry) :
			_reflection(std::move(ptr)), _entry_point_index(entry) {
		}
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
		// TODO get rid of this
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
}
