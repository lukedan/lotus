#pragma once

/// \file
/// Pipeline-related classes.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class device;

	/// A shader.
	class shader : public backend::shader {
	public:
		/// No copy construction.
		shader(const shader&) = delete;
		/// No copy assignment.
		shader &operator=(const shader&) = delete;
	};

	/// A full set of shaders used by a pipeline.
	struct shader_set {
		/// No initialization.
		shader_set(uninitialized_t) {
		}
		/// Creates a \ref shader_set object.
		[[nodiscard]] inline static shader_set create(
			const shader &vert,
			const shader &pix,
			const shader *domain = nullptr,
			const shader *hull = nullptr,
			const shader *geometry = nullptr
		) {
			shader_set result = uninitialized;
			result.vertex_shader = &vert;
			result.pixel_shader = &pix;
			result.domain_shader = domain;
			result.hull_shader = hull;
			result.geometry_shader = geometry;
			return result;
		}

		const shader *vertex_shader;
		const shader *pixel_shader;
		const shader *domain_shader;
		const shader *hull_shader;
		const shader *geometry_shader;
	};

	/// Resources (textures, buffers, etc.) used by a rendering pipeline.
	class pipeline_resources : public backend::pipeline_resources {
		friend device;
	public:
		/// No copy construction.
		pipeline_resources(const pipeline_resources&) = delete;
		/// No copy assignment.
		pipeline_resources &operator=(const pipeline_resources&) = delete;
	protected:
		/// Initializes the base \ref pipeline_resources.
		pipeline_resources(backend::pipeline_resources base) : backend::pipeline_resources(std::move(base)) {
		}
	};

	/// Describes the full state of the pipeline.
	class pipeline_state : public backend::pipeline_state {
		friend device;
	public:
		/// No copy construction.
		pipeline_state(const pipeline_state&) = delete;
		/// No copy assignment.
		pipeline_state &operator=(const pipeline_state&) = delete;
	protected:
		/// Initializes the base \ref pipeline_state.
		pipeline_state(backend::pipeline_state base) : backend::pipeline_state(std::move(base)) {
		}
	};
}
