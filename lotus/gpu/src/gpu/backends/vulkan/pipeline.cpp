#include "lotus/gpu/backends/vulkan/pipeline.h"

/// \file
/// Implementation of pipeline-related functions.

#include "lotus/utils/stack_allocator.h"

namespace lotus::gpu::backends::vulkan {
	std::optional<shader_resource_binding> shader_reflection::find_resource_binding_by_name(
		const char8_t *name
	) const {
		const auto &module = _reflection->GetShaderModule();
		const auto &entry = module.entry_points[_entry_point_index];
		auto past_last_uniform = entry.used_uniforms + entry.used_uniform_count;
		for (std::uint32_t i = 0; i < module.descriptor_binding_count; ++i) {
			const auto it = std::lower_bound(
				entry.used_uniforms,
				past_last_uniform,
				module.descriptor_bindings[i].spirv_id
			);
			if (it == past_last_uniform) {
				continue;
			}
			if (std::strcmp(module.descriptor_bindings[i].name, reinterpret_cast<const char*>(name)) == 0) {
				return _details::conversions::back_to_shader_resource_binding(module.descriptor_bindings[i]);
			}
		}
		return std::nullopt;
	}

	std::size_t shader_reflection::get_output_variable_count() const {
		return _reflection->GetShaderModule().entry_points[_entry_point_index].output_variable_count;
	}

	cvec3s shader_reflection::get_thread_group_size() const {
		const auto &size = _reflection->GetShaderModule().entry_points[_entry_point_index].local_size;
		return cvec3s(size.x, size.y, size.z);
	}


	shader_reflection shader_library_reflection::find_shader(std::u8string_view entry, shader_stage stage) const {
		auto count = _reflection->GetEntryPointCount();
		auto stage_bits = static_cast<SpvReflectShaderStageFlagBits>(static_cast<VkShaderStageFlags>(
			_details::conversions::to_shader_stage_flags(stage)
		));
		for (std::uint32_t i = 0; i < count; ++i) {
			if (stage_bits != _reflection->GetEntryPointShaderStage(i)) {
				continue;
			}
			auto entry_name = _reflection->GetEntryPointName(i);
			auto length = std::strlen(entry_name);
			if (length != entry.size()) {
				continue;
			}
			if (std::strncmp(entry_name, reinterpret_cast<const char*>(entry.data()), length) == 0) {
				return shader_reflection(_reflection, i);
			}
		}
		return nullptr;
	}
}
