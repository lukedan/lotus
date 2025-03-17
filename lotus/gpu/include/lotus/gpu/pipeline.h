#pragma once

/// \file
/// Pipeline-related classes.

#include LOTUS_GPU_BACKEND_INCLUDE_COMMON
#include LOTUS_GPU_BACKEND_INCLUDE_PIPELINE

namespace lotus::gpu {
	class device;
	class shader_utility;
	class shader_library_reflection;

	/// Shader reflection.
	class shader_reflection : public backend::shader_reflection {
		friend shader_utility;
		friend shader_library_reflection;
	public:
		/// Creates an empty object.
		shader_reflection(std::nullptr_t) : backend::shader_reflection(nullptr) {
		}
		/// Move constructor.
		shader_reflection(shader_reflection &&src) : backend::shader_reflection(std::move(src)) {
		}
		/// Copy constructor.
		shader_reflection(const shader_reflection &src) : backend::shader_reflection(src) {
		}
		/// Move assignment.
		shader_reflection &operator=(shader_reflection &&src) {
			backend::shader_reflection::operator=(std::move(src));
			return *this;
		}
		/// Copy assignment.
		shader_reflection &operator=(const shader_reflection &src) {
			backend::shader_reflection::operator=(src);
			return *this;
		}

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
		/// Returns the number of shader resource bindings.
		[[nodiscard]] u32 get_resource_binding_count() const {
			return backend::shader_reflection::get_resource_binding_count();
		}
		/// Returns the shader resource binding at the given index.
		[[nodiscard]] shader_resource_binding get_resource_binding_at_index(u32 i) const {
			return backend::shader_reflection::get_resource_binding_at_index(i);
		}

		/// Returns the number of render targets for a pixel shader.
		[[nodiscard]] u32 get_render_target_count() const {
			return backend::shader_reflection::get_render_target_count();
		}

		/// Returns the thread group size of this shader if it's a compute shader.
		[[nodiscard]] cvec3u32 get_thread_group_size() const {
			return backend::shader_reflection::get_thread_group_size();
		}

		/// Returns whether this object contains a valid handle.
		[[nodiscard]] bool is_valid() const {
			return backend::shader_reflection::is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	protected:
		/// Initializes the base object.
		shader_reflection(backend::shader_reflection &&base) : backend::shader_reflection(std::move(base)) {
		}
	};

	/// Reflection of a set of shaders compiled into a single binary.
	class shader_library_reflection : public backend::shader_library_reflection {
		friend shader_utility;
	public:
		/// Initializes this object to empty.
		shader_library_reflection(std::nullptr_t) : backend::shader_library_reflection(nullptr) {
		}
		/// Move constructor.
		shader_library_reflection(shader_library_reflection &&src) :
			backend::shader_library_reflection(std::move(src)) {
		}
		/// Move assignment.
		shader_library_reflection &operator=(shader_library_reflection &&src) {
			backend::shader_library_reflection::operator=(std::move(src));
			return *this;
		}

		/// Returns the number of shaders contained by this shader library.
		[[nodiscard]] u32 get_num_shaders() const {
			return backend::shader_library_reflection::get_num_shaders();
		}
		/// Returns the i-th shader in this shader library.
		[[nodiscard]] shader_reflection get_shader_at(u32 i) const {
			return backend::shader_library_reflection::get_shader_at(i);
		}
		/// Finds a shader that matches the given entry name and stage. If none is found, returns an empty object.
		[[nodiscard]] shader_reflection find_shader(std::u8string_view entry, shader_stage stage) const {
			return backend::shader_library_reflection::find_shader(entry, stage);
		}
	protected:
		/// Initializes the base object.
		shader_library_reflection(backend::shader_library_reflection &&base) :
			backend::shader_library_reflection(std::move(base)) {
		}
	};


	/// Shader binary that contains one shader, or a collection of raytracing shaders.
	class shader_binary : public backend::shader_binary {
		friend device;
	public:
		/// Creates an empty object.
		shader_binary(std::nullptr_t) : backend::shader_binary(nullptr) {
		}
		/// Move constructor.
		shader_binary(shader_binary &&src) : backend::shader_binary(std::move(src)) {
		}
		/// No copy construction.
		shader_binary(const shader_binary&) = delete;
		/// Move assignment.
		shader_binary &operator=(shader_binary &&src) {
			backend::shader_binary::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		shader_binary &operator=(const shader_binary&) = delete;
	protected:
		/// Initializes the base object.
		shader_binary(backend::shader_binary &&base) : backend::shader_binary(std::move(base)) {
		}
	};

	/// A full set of shaders used by a pipeline.
	struct shader_set {
		/// Initializes all shaders to empty.
		shader_set(std::nullptr_t) {
		}
		/// Initializes all fields of this struct.
		shader_set(
			const shader_binary &vert,
			const shader_binary &pix,
			const shader_binary *domain = nullptr,
			const shader_binary *hull = nullptr,
			const shader_binary *geometry = nullptr
		) :
			vertex_shader(&vert),
			pixel_shader(&pix),
			domain_shader(domain),
			hull_shader(hull),
			geometry_shader(geometry) {
		}

		const shader_binary *vertex_shader   = nullptr; ///< Vertex shader.
		const shader_binary *pixel_shader    = nullptr; ///< Pixel shader.
		const shader_binary *domain_shader   = nullptr; ///< Domain shader.
		const shader_binary *hull_shader     = nullptr; ///< Hull shader.
		const shader_binary *geometry_shader = nullptr; ///< Geometry shader.
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

	/// Describes the full state of the raytracing pipeline.
	class raytracing_pipeline_state : public backend::raytracing_pipeline_state {
		friend device;
	public:
		/// Creates an empty pipeline state object.
		raytracing_pipeline_state(std::nullptr_t) : backend::raytracing_pipeline_state(nullptr) {
		}
		/// Move construction.
		raytracing_pipeline_state(raytracing_pipeline_state &&src) :
			backend::raytracing_pipeline_state(std::move(src)) {
		}
		/// No copy construction.
		raytracing_pipeline_state(const raytracing_pipeline_state&) = delete;
		/// Move assignment.
		raytracing_pipeline_state &operator=(raytracing_pipeline_state &&src) {
			backend::raytracing_pipeline_state::operator=(std::move(src));
			return *this;
		}
		/// No copy assignment.
		raytracing_pipeline_state &operator=(const raytracing_pipeline_state&) = delete;
	protected:
		/// Initializes the base object.
		raytracing_pipeline_state(backend::raytracing_pipeline_state &&base) :
			backend::raytracing_pipeline_state(std::move(base)) {
		}
	};

	/// Handle of a shader group, used for raytracing.
	class shader_group_handle : public backend::shader_group_handle {
		friend device;
	public:
		/// Initializes the handle to empty.
		shader_group_handle(uninitialized_t) : backend::shader_group_handle(uninitialized) {
		}

		/// Returns the handle data that can be copied to buffers that are used when calling
		/// \ref command_list::trace_rays().
		[[nodiscard]] std::span<const std::byte> data() const {
			return backend::shader_group_handle::data();
		}
	protected:
		/// Initializes the base object.
		shader_group_handle(backend::shader_group_handle &&base) : backend::shader_group_handle(std::move(base)) {
		}
	};

	/// A heap used to store timestamp query results.
	class timestamp_query_heap : public backend::timestamp_query_heap {
		friend device;
	public:
		/// Initializes this heap to empty.
		timestamp_query_heap(std::nullptr_t) : backend::timestamp_query_heap(nullptr) {
		}

		/// Tests if this object holds a valid heap.
		[[nodiscard]] bool is_valid() const {
			return backend::timestamp_query_heap::is_valid();
		}
		/// \overload
		[[nodiscard]] explicit operator bool() const {
			return is_valid();
		}
	private:
		/// Initializes the base object.
		timestamp_query_heap(backend::timestamp_query_heap &&base) : backend::timestamp_query_heap(std::move(base)) {
		}
	};
}
