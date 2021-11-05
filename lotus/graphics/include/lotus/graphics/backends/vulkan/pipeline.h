#pragma once

/// \file
/// Pipelines.

#include <optional>

#include <vulkan/vulkan.hpp>
#include <spirv_reflect.h>

#include "details.h"

namespace lotus::graphics::backends::vulkan {
	class command_list;
	class device;
	class shader;


	/// Contains a \p SpvReflectShaderModule.
	class shader_reflection {
		friend device;
		friend shader;
	public:
		/// Move constructor.
		shader_reflection(shader_reflection &&src) : _reflection(std::exchange(src._reflection, std::nullopt)) {
		}
		/// No copy construction.
		shader_reflection(const shader_reflection&) = delete;
		/// Move assignment.
		shader_reflection &operator=(shader_reflection &&src) {
			if (this != &src) {
				if (_reflection) {
					spvReflectDestroyShaderModule(&_reflection.value());
				}
				_reflection = std::exchange(src._reflection, std::nullopt);
			}
			return *this;
		}
		/// No copy assignment.
		shader_reflection &operator=(const shader_reflection&) = delete;
		/// Destroys the reflection data if necessary.
		~shader_reflection() {
			if (_reflection) {
				spvReflectDestroyShaderModule(&_reflection.value());
			}
		}
	protected:
		/// Creates an empty object.
		shader_reflection(std::nullptr_t) {
		}
	private:
		std::optional<SpvReflectShaderModule> _reflection; ///< Reflection data.
	};
	
	/// Contains a \p vk::UniqueShaderModule.
	class shader {
		friend device;
	protected:
		/// Creates an empty object.
		shader(std::nullptr_t) : _reflection(nullptr) {
		}
	private:
		vk::UniqueShaderModule _module; ///< The shader module.
		// TODO get rid of this
		shader_reflection _reflection; ///< Reflection data.
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
