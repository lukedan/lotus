#include "lotus/gpu/backends/vulkan/pipeline.h"

/// \file
/// Implementation of pipeline-related functions.

#include "lotus/utils/stack_allocator.h"

namespace lotus::gpu::backends::vulkan {
	std::optional<shader_resource_binding> shader_reflection::find_resource_binding_by_name(
		const char8_t *name
	) const {
		auto bookmark = stack_allocator::for_this_thread().bookmark();

		std::uint32_t count;
		_details::assert_spv_reflect(_reflection.EnumerateDescriptorBindings(&count, nullptr));
		auto bindings = bookmark.create_vector_array<SpvReflectDescriptorBinding*>(count);
		_details::assert_spv_reflect(_reflection.EnumerateDescriptorBindings(&count, bindings.data()));

		std::u8string_view name_view(name);
		for (std::uint32_t i = 0; i < count; ++i) {
			if (reinterpret_cast<const char8_t*>(bindings[i]->name) == name_view) {
				return _details::conversions::back_to_shader_resource_binding(*bindings[i]);
			}
		}
		return std::nullopt;
	}

	std::size_t shader_reflection::get_output_variable_count() const {
		std::uint32_t count;
		_details::assert_spv_reflect(_reflection.EnumerateOutputVariables(&count, nullptr));
		return count;
	}
}
