#pragma once

#include <vector>

#include <lotus/gpu/resources.h>

#include "common.h"

struct scene_resources {
	[[nodiscard]] inline static scene_resources create(
		lgpu::device &dev, const lgpu::adapter_properties &dev_props,
		lgpu::command_allocator &cmd_alloc, lgpu::command_queue &cmd_queue,
		lgpu::descriptor_pool &descriptor_pool
	) {
		auto upload_fence = dev.create_fence(lgpu::synchronization_state::unset);

		{ // instances
			std::size_t instance_buf_size = sizeof(instance_data) * result.instances.size();
			result.instance_buffer = dev.create_committed_buffer(instance_buf_size, lgpu::heap_type::device_only, lgpu::buffer_usage::mask::copy_destination | lgpu::buffer_usage::mask::read_only_buffer);
			auto upload_buf = dev.create_committed_buffer(instance_buf_size, lgpu::heap_type::upload, lgpu::buffer_usage::mask::copy_source);
			void *ptr = dev.map_buffer(upload_buf, 0, 0);
			std::memcpy(ptr, result.instances.data(), instance_buf_size);
			dev.unmap_buffer(upload_buf, 0, instance_buf_size);
			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.copy_buffer(upload_buf, 0, result.instance_buffer, 0, instance_buf_size);
			copy_cmd.resource_barrier(
				{},
				{
					lgpu::buffer_barrier::create(result.instance_buffer, lgpu::buffer_usage::copy_destination, lgpu::buffer_usage::read_only_buffer),
				}
			);
			copy_cmd.finish();
			cmd_queue.submit_command_lists({ &copy_cmd }, &upload_fence);
			dev.wait_for_fence(upload_fence);
			dev.reset_fence(upload_fence);
		}

		// top level acceleration structure
		std::size_t num_instances = 0;
		for (const auto &prim : model.nodes) {
			if (prim.mesh < 0) {
				continue;
			}
			num_instances += model.meshes[prim.mesh].primitives.size();
		}
		std::size_t tlas_buf_size = num_instances * sizeof(lgpu::instance_description);
		auto tlas_buf = dev.create_committed_buffer(tlas_buf_size, lgpu::heap_type::upload, lgpu::buffer_usage::mask::read_only_buffer);
		auto *ptr = static_cast<lgpu::instance_description*>(dev.map_buffer(tlas_buf, 0, 0));
		std::size_t instance_i = 0;
		for (const auto &node : model.nodes) {
			if (node.mesh < 0) {
				continue;
			}
			auto &mesh = model.meshes[node.mesh];
			auto transform = mat44f::identity();
			if (!node.matrix.empty()) {
				for (std::size_t row = 0; row < 4; ++row) {
					for (std::size_t col = 0; col < 4; ++col) {
						transform(row, col) = node.matrix[row * 4 + col];
					}
				}
			}
			for (std::size_t prim_i = 0; prim_i < mesh.primitives.size(); ++prim_i) {
				const auto &prim = mesh.primitives[prim_i];
				ptr[instance_i] = dev.get_bottom_level_acceleration_structure_description(
					result.blas[node.mesh][prim_i], transform, result.instance_indices[node.mesh][prim_i], 0xFFu, prim.indices < 0 ? 1 : 0
				);
				++instance_i;
			}
		}
		assert(instance_i == num_instances);
		dev.unmap_buffer(tlas_buf, 0, tlas_buf_size);
		auto tlas_sizes = dev.get_top_level_acceleration_structure_build_sizes(tlas_buf, 0, num_instances);
		result.tlas_buffer = dev.create_committed_buffer(tlas_sizes.acceleration_structure_size, lgpu::heap_type::device_only, lgpu::buffer_usage::mask::acceleration_structure);
		result.tlas = dev.create_top_level_acceleration_structure(result.tlas_buffer, 0, tlas_sizes.acceleration_structure_size);
		{
			auto tlas_scratch = dev.create_committed_buffer(tlas_sizes.build_scratch_size, lgpu::heap_type::device_only, lgpu::buffer_usage::mask::read_write_buffer);
			auto cmd_list = dev.create_and_start_command_list(cmd_alloc);
			cmd_list.resource_barrier({}, { lgpu::buffer_barrier::create(tlas_scratch, lgpu::buffer_usage::read_only_buffer, lgpu::buffer_usage::read_write_buffer)});
			cmd_list.build_acceleration_structure(tlas_buf, 0, num_instances, result.tlas, tlas_scratch, 0);
			cmd_list.finish();
			auto fence = dev.create_fence(lgpu::synchronization_state::unset);
			cmd_queue.submit_command_lists({ &cmd_list }, &fence);
			dev.wait_for_fence(fence);
		}

		return result;
	}

	lgpu::image2d empty_color = nullptr;
	lgpu::image2d empty_normal = nullptr;
	lgpu::image2d empty_metalness_glossiness = nullptr;
	std::size_t empty_color_view_index;
	std::size_t empty_normal_view_index;
	std::size_t empty_metalness_glossiness_view_index;

	std::vector<std::vector<std::size_t>> instance_indices;

	lgpu::buffer vertex_buffer = nullptr;
	std::size_t vertex_count;
	lgpu::buffer index_buffer = nullptr;
	std::size_t index_count;
	lgpu::buffer instance_buffer = nullptr;
	lgpu::buffer material_buffer = nullptr;

	lgpu::descriptor_set textures_descriptor_set = nullptr;
	std::vector<lgpu::descriptor_set> material_descriptor_sets;
	std::vector<lgpu::descriptor_set> node_descriptor_sets;
	lgpu::buffer node_buffer = nullptr;
	std::size_t aligned_node_data_size;
	lgpu::buffer material_uniform_buffer = nullptr;
	std::size_t aligned_material_data_size;
	lgpu::descriptor_set_layout textures_descriptor_layout = nullptr;
	lgpu::descriptor_set_layout material_descriptor_layout = nullptr;
	lgpu::descriptor_set_layout node_descriptor_layout = nullptr;

	std::vector<std::vector<lgpu::bottom_level_acceleration_structure>> blas;
	std::vector<std::vector<lgpu::buffer>> blas_buffers;
	lgpu::top_level_acceleration_structure tlas = nullptr;
	lgpu::buffer tlas_buffer = nullptr;
};
