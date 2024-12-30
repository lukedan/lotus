#pragma once

/// \file
/// Metal shader utilities and render pipelines.

#include <cstddef>

namespace lotus::gpu::backends::metal {
	// TODO
	class shader_reflection {
	protected:
		shader_reflection(std::nullptr_t); // TODO

		[[nodiscard]] std::optional<shader_resource_binding> find_resource_binding_by_name(const char8_t*) const; // TODO

		template <typename Callback> void enumerate_resource_bindings(Callback &&cb) const; // TODO

		[[nodiscard]] std::size_t get_output_variable_count() const; // TODO

		template <typename Callback> void enumerate_output_variables(Callback &&cb) const; // TODO

		[[nodiscard]] cvec3s get_thread_group_size() const; // TODO

		[[nodiscard]] bool is_valid() const; // TODO
	};

	// TODO
	class shader_library_reflection {
	protected:
		shader_library_reflection(std::nullptr_t); // TODO

		template <typename Callback> void enumerate_shaders(Callback &&cb) const; // TODO

		[[nodiscard]] shader_reflection find_shader(std::u8string_view entry, shader_stage) const; // TODO
	};

	// TODO
	class shader_binary {
	protected:
		shader_binary(std::nullptr_t); // TODO
	};

	// TODO
	class pipeline_resources {
	protected:
		pipeline_resources(std::nullptr_t); // TODO
	};

	// TODO
	class graphics_pipeline_state {
	protected:
		graphics_pipeline_state(std::nullptr_t); // TODO
	};

	// TODO
	class compute_pipeline_state {
	protected:
		compute_pipeline_state(std::nullptr_t); // TODO
	};

	// TODO
	class raytracing_pipeline_state {
	protected:
		raytracing_pipeline_state(std::nullptr_t); // TODO
	};

	// TODO
	class shader_group_handle {
	protected:
		shader_group_handle(uninitialized_t); // TODO

		[[nodiscard]] std::span<const std::byte> data() const; // TODO
	};

	// TODO
	class timestamp_query_heap {
	protected:
		timestamp_query_heap(std::nullptr_t); // TODO

		[[nodiscard]] bool is_valid() const; // TODO
	};
}
