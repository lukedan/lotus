#include "lotus/gpu/backends/vulkan/pipeline.h"

/// \file
/// Implementation of pipeline-related functions.

#include "lotus/memory/stack_allocator.h"

namespace lotus::gpu::backends::vulkan {
	std::optional<shader_resource_binding> shader_reflection::find_resource_binding_by_name(
		const char8_t *name
	) const {
		for (const shader_resource_binding &binding : _cache->descriptor_bindings) {
			if (binding.name == name) {
				return binding;
			}
		}
		return std::nullopt;
	}

	std::uint32_t shader_reflection::get_resource_binding_count() const {
		return static_cast<std::uint32_t>(_cache->descriptor_bindings.size());
	}

	shader_resource_binding shader_reflection::get_resource_binding_at_index(std::uint32_t i) const {
		return _cache->descriptor_bindings[i];
	}

	std::uint32_t shader_reflection::get_render_target_count() const {
		return _reflection->GetShaderModule().entry_points[_entry_point_index].output_variable_count;
	}

	cvec3s shader_reflection::get_thread_group_size() const {
		const auto &size = _reflection->GetShaderModule().entry_points[_entry_point_index].local_size;
		return cvec3s(size.x, size.y, size.z);
	}

	shader_reflection::shader_reflection(std::shared_ptr<spv_reflect::ShaderModule> ptr, std::uint32_t entry) :
		_reflection(std::move(ptr)), _entry_point_index(entry) {

		if (_reflection) {
			_cache = std::make_unique<_cached_data>();
			const SpvReflectShaderModule &module = _reflection->GetShaderModule();
			const SpvReflectEntryPoint &entry = module.entry_points[_entry_point_index];
			const uint32_t *uniforms_begin = entry.used_uniforms;
			const uint32_t *uniforms_end = uniforms_begin + entry.used_uniform_count;
			for (std::uint32_t i = 0; i < module.descriptor_binding_count; ++i) {
				const SpvReflectDescriptorBinding &binding = module.descriptor_bindings[i];
				const auto it = std::lower_bound(uniforms_begin, uniforms_end, binding.spirv_id);
				if (it == uniforms_end) {
					continue;
				}
				_cache->descriptor_bindings.emplace_back(
					_details::conversions::back_to_shader_resource_binding(binding)
				);
			}
		}
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
