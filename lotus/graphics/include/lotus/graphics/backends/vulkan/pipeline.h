#pragma once

/// \file
/// Pipelines.

#include <optional>

#include <vulkan/vulkan.hpp>
#include <spirv_reflect.h>

#include "details.h"
#include "lotus/utils/stack_allocator.h"

namespace lotus::graphics::backends::vulkan {
	class command_list;
	class device;
	class shader_utility;


	/// Contains a \p SpvReflectShaderModule.
	class shader_reflection {
		friend device;
		friend shader_utility;
	protected:
		/// Creates an empty object.
		shader_reflection(std::nullptr_t) {
		}

		/// Iterates through the bindings and returns the one with the specified name.
		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(const char8_t*) const;
		/// Gets all \p SpvReflectDescriptorBinding using \p spv_reflect::ShaderModule::EnumerateDescriptorBindings()
		/// and then enumerates over them.
		template <typename Callback> void enumerate_resource_bindings(Callback &&cb) const {
			auto bookmark = stack_allocator::for_this_thread().bookmark();

			std::uint32_t count;
			_details::assert_spv_reflect(_reflection.EnumerateDescriptorBindings(&count, nullptr));
			auto bindings = bookmark.create_vector_array<SpvReflectDescriptorBinding*>(count);
			_details::assert_spv_reflect(_reflection.EnumerateDescriptorBindings(&count, bindings.data()));

			for (std::uint32_t i = 0; i < count; ++i) {
				if (!cb(_details::conversions::back_to_shader_resource_binding(*bindings[i]))) {
					break;
				}
			}
		}
	private:
		spv_reflect::ShaderModule _reflection; ///< Reflection data.
	};
	
	/// Contains a \p vk::UniqueShaderModule.
	class shader {
		friend device;
	protected:
		/// Creates an empty object.
		shader(std::nullptr_t) {
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
}
