#pragma once

#include <vector>

#include <lotus/graphics/resources.h>

#include "common.h"

struct scene_resources {
	[[nodiscard]] inline static scene_resources create(
		gfx::device &dev, const gfx::adapter_properties &dev_props,
		gfx::command_allocator &cmd_alloc, gfx::command_queue &cmd_queue,
		gfx::descriptor_pool &descriptor_pool
	) {
		auto upload_fence = dev.create_fence(gfx::synchronization_state::unset);

		{ // instances
			std::size_t instance_buf_size = sizeof(instance_data) * result.instances.size();
			result.instance_buffer = dev.create_committed_buffer(instance_buf_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::copy_destination | gfx::buffer_usage::mask::read_only_buffer);
			auto upload_buf = dev.create_committed_buffer(instance_buf_size, gfx::heap_type::upload, gfx::buffer_usage::mask::copy_source);
			void *ptr = dev.map_buffer(upload_buf, 0, 0);
			std::memcpy(ptr, result.instances.data(), instance_buf_size);
			dev.unmap_buffer(upload_buf, 0, instance_buf_size);
			auto copy_cmd = dev.create_and_start_command_list(cmd_alloc);
			copy_cmd.copy_buffer(upload_buf, 0, result.instance_buffer, 0, instance_buf_size);
			copy_cmd.resource_barrier(
				{},
				{
					gfx::buffer_barrier::create(result.instance_buffer, gfx::buffer_usage::copy_destination, gfx::buffer_usage::read_only_buffer),
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
		std::size_t tlas_buf_size = num_instances * sizeof(gfx::instance_description);
		auto tlas_buf = dev.create_committed_buffer(tlas_buf_size, gfx::heap_type::upload, gfx::buffer_usage::mask::read_only_buffer);
		auto *ptr = static_cast<gfx::instance_description*>(dev.map_buffer(tlas_buf, 0, 0));
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
		result.tlas_buffer = dev.create_committed_buffer(tlas_sizes.acceleration_structure_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::acceleration_structure);
		result.tlas = dev.create_top_level_acceleration_structure(result.tlas_buffer, 0, tlas_sizes.acceleration_structure_size);
		{
			auto tlas_scratch = dev.create_committed_buffer(tlas_sizes.build_scratch_size, gfx::heap_type::device_only, gfx::buffer_usage::mask::read_write_buffer);
			auto cmd_list = dev.create_and_start_command_list(cmd_alloc);
			cmd_list.resource_barrier({}, { gfx::buffer_barrier::create(tlas_scratch, gfx::buffer_usage::read_only_buffer, gfx::buffer_usage::read_write_buffer)});
			cmd_list.build_acceleration_structure(tlas_buf, 0, num_instances, result.tlas, tlas_scratch, 0);
			cmd_list.finish();
			auto fence = dev.create_fence(gfx::synchronization_state::unset);
			cmd_queue.submit_command_lists({ &cmd_list }, &fence);
			dev.wait_for_fence(fence);
		}

		return result;
	}

	template <
		typename Callback
	> [[nodiscard]] inline static std::vector<std::vector<gfx::graphics_pipeline_state>> create_pipeline_states(
		const gltf::Model &model, Callback &&create_pipeline
	) {
		std::vector<std::vector<gfx::graphics_pipeline_state>> pipelines;
		pipelines.reserve(model.meshes.size());
		{
			std::vector<gfx::input_buffer_element> vert_buffer_elements;
			std::vector<gfx::input_buffer_layout> vert_buffer_layouts;
			for (const auto &mesh : model.meshes) {
				auto &prim_pipelines = pipelines.emplace_back();
				prim_pipelines.reserve(mesh.primitives.size());
				for (const auto &prim : mesh.primitives) {
					vert_buffer_elements.clear();
					vert_buffer_elements.reserve(prim.attributes.size());
					vert_buffer_layouts.clear();
					vert_buffer_layouts.reserve(prim.attributes.size());
					for (auto it = prim.attributes.begin(); it != prim.attributes.end(); ++it) {
						const auto &attr = *it;
						const auto &accessor = model.accessors[attr.second];
						gfx::input_buffer_element elem = uninitialized;
						if (attr.first == "POSITION") {
							elem = gfx::input_buffer_element::create(
								u8"POSITION", 0, gfx::format::r32g32b32_float, 0
							);
						} else if (attr.first == "NORMAL") {
							elem = gfx::input_buffer_element::create(
								u8"NORMAL", 0, gfx::format::r32g32b32_float, 0
							);
						} else if (attr.first == "TANGENT") {
							elem = gfx::input_buffer_element::create(
								u8"TANGENT", 0, gfx::format::r32g32b32a32_float, 0
							);
						} else if (attr.first == "TEXCOORD_0") {
							gfx::format fmt;
							switch (accessor.componentType) {
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
								fmt = gfx::format::r8g8_unorm;
								break;
							case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
								fmt = gfx::format::r16g16_unorm;
								break;
							case TINYGLTF_COMPONENT_TYPE_FLOAT:
								fmt = gfx::format::r32g32_float;
								break;
							default:
								std::cerr << "Unhandled texcoord format: " << accessor.componentType << std::endl;
								continue;
							}
							elem = gfx::input_buffer_element::create(u8"TEXCOORD", 0, fmt, 0);
						} else {
							std::cerr << "Unhandled vertex buffer element: " << attr.first << std::endl;
							continue;
						}
						std::size_t index = vert_buffer_elements.size();
						vert_buffer_elements.emplace_back(elem);
						vert_buffer_layouts.emplace_back(gfx::input_buffer_layout::create_vertex_buffer(
							{ vert_buffer_elements.end() - 1, vert_buffer_elements.end() },
							accessor.ByteStride(model.bufferViews[accessor.bufferView]), index
						));
					}
					prim_pipelines.emplace_back(create_pipeline(vert_buffer_layouts));
				}
			}
		}
		return pipelines;
	}

	gfx::image2d empty_color = nullptr;
	gfx::image2d empty_normal = nullptr;
	gfx::image2d empty_metalness_glossiness = nullptr;
	std::size_t empty_color_view_index;
	std::size_t empty_normal_view_index;
	std::size_t empty_metalness_glossiness_view_index;

	std::vector<std::vector<std::size_t>> instance_indices;

	gfx::buffer vertex_buffer = nullptr;
	std::size_t vertex_count;
	gfx::buffer index_buffer = nullptr;
	std::size_t index_count;
	gfx::buffer instance_buffer = nullptr;
	gfx::buffer material_buffer = nullptr;

	gfx::descriptor_set textures_descriptor_set = nullptr;
	std::vector<gfx::descriptor_set> material_descriptor_sets;
	std::vector<gfx::descriptor_set> node_descriptor_sets;
	gfx::buffer node_buffer = nullptr;
	std::size_t aligned_node_data_size;
	gfx::buffer material_uniform_buffer = nullptr;
	std::size_t aligned_material_data_size;
	gfx::descriptor_set_layout textures_descriptor_layout = nullptr;
	gfx::descriptor_set_layout material_descriptor_layout = nullptr;
	gfx::descriptor_set_layout node_descriptor_layout = nullptr;

	std::vector<std::vector<gfx::bottom_level_acceleration_structure>> blas;
	std::vector<std::vector<gfx::buffer>> blas_buffers;
	gfx::top_level_acceleration_structure tlas = nullptr;
	gfx::buffer tlas_buffer = nullptr;
};
