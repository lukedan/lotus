#pragma once

/// \file
/// Pipeline-related classes.

#include LOTUS_GRAPHICS_BACKEND_INCLUDE

namespace lotus::graphics {
	class device;
	class shader_utility;

	/// Shader reflection.
	class shader_reflection : public backend::shader_reflection {
		friend shader_utility;
	public:
		/// Creates an empty object.
		shader_reflection(std::nullptr_t) : backend::shader_reflection(nullptr) {
		}
		/// Move constructor.
		shader_reflection(shader_reflection &&src) : backend::shader_reflection(std::move(src)) {
		}
		/// No copy construction.
		shader_reflection(const shader_reflection&) = delete;
		/// Move assignment.
		shader_reflection &operator=(shader_reflection &&src) {
			backend::shader_reflection::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		shader_reflection &operator=(const shader_reflection&) = delete;

		/// Finds the binding with the specified name.
		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(
			const char8_t *name
		) const {
			return backend::shader_reflection::find_resource_binding_by_name(name);
		}
		/// \overload
		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(
			const std::u8string &name
		) const {
			return find_resource_binding_by_name(name.c_str());
		}
		/// Calls the given callback function for each resource binding. The callback function can return \p false to
		/// stop the enumeration early.
		template <typename Callback> void enumerate_resource_bindings(Callback &&cb) {
			backend::shader_reflection::enumerate_resource_bindings(
				[&](const shader_resource_binding &binding) {
					using _result_t = std::invoke_result_t<Callback&&, const shader_resource_binding&>;
					if constexpr (std::is_same_v<_result_t, bool>) {
						return cb(binding);
					} else {
						static_assert(std::is_same_v<_result_t, void>, "Callback must return bool or nothing");
						cb(binding);
						return true;
					}
				}
			);
		}
	protected:
		/// Initializes the base object.
		shader_reflection(backend::shader_reflection &&base) : backend::shader_reflection(std::move(base)) {
		}
	};

	/// A shader.
	class shader : public backend::shader {
		friend device;
	public:
		/// Creates an empty shader object.
		shader(std::nullptr_t) : backend::shader(nullptr) {
		}
		/// Move constructor.
		shader(shader &&src) : backend::shader(std::move(src)) {
		}
		/// No copy construction.
		shader(const shader&) = delete;
		/// Move assignment.
		shader &operator=(shader &&src) {
			backend::shader::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		shader &operator=(const shader&) = delete;
	protected:
		/// Initializes the base object.
		shader(backend::shader &&base) : backend::shader(std::move(base)) {
		}
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
		/// Creates an empty object.
		pipeline_resources(std::nullptr_t) : backend::pipeline_resources(nullptr) {
		}
		/// Move constructor.
		pipeline_resources(pipeline_resources &&src) : backend::pipeline_resources(std::move(src)) {
		}
		/// No copy construction.
		pipeline_resources(const pipeline_resources&) = delete;
		/// Move assignment.
		pipeline_resources &operator=(pipeline_resources &&src) {
			backend::pipeline_resources::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		pipeline_resources &operator=(const pipeline_resources&) = delete;
	protected:
		/// Initializes the base \ref pipeline_resources.
		pipeline_resources(backend::pipeline_resources base) : backend::pipeline_resources(std::move(base)) {
		}
	};

	/// Describes the full state of the graphics pipeline.
	class graphics_pipeline_state : public backend::graphics_pipeline_state {
		friend device;
	public:
		/// Creates an empty pipeline state object.
		graphics_pipeline_state(std::nullptr_t) : backend::graphics_pipeline_state(nullptr) {
		}
		/// Move constructor.
		graphics_pipeline_state(graphics_pipeline_state &&src) : backend::graphics_pipeline_state(std::move(src)) {
		}
		/// No copy construction.
		graphics_pipeline_state(const graphics_pipeline_state&) = delete;
		/// Move assignment.
		graphics_pipeline_state &operator=(graphics_pipeline_state &&src) {
			backend::graphics_pipeline_state::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		graphics_pipeline_state &operator=(const graphics_pipeline_state&) = delete;
	protected:
		/// Initializes the base \ref graphics_pipeline_state.
		graphics_pipeline_state(backend::graphics_pipeline_state base) :
			backend::graphics_pipeline_state(std::move(base)) {
		}
	};

	/// Describes the full state of the compute pipeline.
	class compute_pipeline_state : public backend::compute_pipeline_state {
		friend device;
	public:
		/// Creates an empty pipeline state object.
		compute_pipeline_state(std::nullptr_t) : backend::compute_pipeline_state(nullptr) {
		}
		/// Move constructor.
		compute_pipeline_state(compute_pipeline_state &&src) : backend::compute_pipeline_state(std::move(src)) {
		}
		/// No copy construction.
		compute_pipeline_state(const compute_pipeline_state&) = delete;
		/// Move assignment.
		compute_pipeline_state &operator=(compute_pipeline_state &&src) {
			backend::compute_pipeline_state::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		compute_pipeline_state &operator=(const compute_pipeline_state&) = delete;
	protected:
		/// Initializes the base \ref compute_pipeline_state.
		compute_pipeline_state(backend::compute_pipeline_state base) :
			backend::compute_pipeline_state(std::move(base)) {
		}
	};
}
