#pragma once

#include "scene.h"

class raytrace_pass {
public:
	struct global_data {
		global_data(uninitialized_t) {
		}

		cvec3f camera_position = uninitialized;
		float t_min;
		cvec3f top_left = uninitialized;
		float t_max;
		cvec4f right = uninitialized;
		cvec3f down = uninitialized;
		std::uint32_t frame_index;
	};
	struct input_resources {
		lgpu::buffer constant_buffer = nullptr;
		lgpu::descriptor_set descriptors = nullptr;
		cvec2s output_size = uninitialized;
	};

	raytrace_pass(lgpu::device &dev, const scene_resources &s, const lgpu::adapter_properties &prop) {
		auto shader_bin = load_binary_file("shaders/raytracing.lib.o");
		_shaders = dev.load_shader(shader_bin);

		_rt_descriptor_layout = dev.create_descriptor_set_layout(
			{
				lgpu::descriptor_range_binding::create(lgpu::descriptor_type::acceleration_structure, 1, 0),
				lgpu::descriptor_range_binding::create(lgpu::descriptor_type::constant_buffer, 1, 1),
				lgpu::descriptor_range_binding::create(lgpu::descriptor_type::read_write_image, 1, 2),
				lgpu::descriptor_range_binding::create(lgpu::descriptor_type::read_only_buffer, 4, 3),
				lgpu::descriptor_range_binding::create(lgpu::descriptor_type::sampler, 1, 7),
			},
			lgpu::shader_stage::all
		);
		_pipeline_resources = dev.create_pipeline_resources({ &s.textures_descriptor_layout, &_rt_descriptor_layout });

		_pipeline_state = dev.create_raytracing_pipeline_state(
			{
				lgpu::shader_function::create(_shaders, u8"main_closesthit_indexed", lgpu::shader_stage::closest_hit_shader),
				lgpu::shader_function::create(_shaders, u8"main_closesthit_unindexed", lgpu::shader_stage::closest_hit_shader),
				lgpu::shader_function::create(_shaders, u8"main_anyhit_indexed", lgpu::shader_stage::any_hit_shader),
				lgpu::shader_function::create(_shaders, u8"main_anyhit_unindexed", lgpu::shader_stage::any_hit_shader),
			},
			{
				lgpu::hit_shader_group::create(0, 2),
				lgpu::hit_shader_group::create(1, 3),
			},
			{
				lgpu::shader_function::create(_shaders, u8"main_miss", lgpu::shader_stage::miss_shader),
				lgpu::shader_function::create(_shaders, u8"main_raygen", lgpu::shader_stage::ray_generation_shader),
			},
			20, 32, 8, _pipeline_resources
		);

		_shader_group_handle_size = memory::align_size(prop.shader_group_handle_size, prop.shader_group_handle_alignment);
		_raygen_buffer = dev.create_committed_buffer(_shader_group_handle_size, lgpu::heap_type::upload, lgpu::buffer_usage::mask::read_only_buffer);
		_miss_buffer = dev.create_committed_buffer(_shader_group_handle_size, lgpu::heap_type::upload, lgpu::buffer_usage::mask::read_only_buffer);
		_hit_group_buffer = dev.create_committed_buffer(_shader_group_handle_size * 2, lgpu::heap_type::upload, lgpu::buffer_usage::mask::read_only_buffer);
		auto *raygen_rec = static_cast<std::byte*>(dev.map_buffer(_raygen_buffer, 0, 0));
		auto *miss_rec = static_cast<std::byte*>(dev.map_buffer(_miss_buffer, 0, 0));
		auto *hit_group_rec = static_cast<std::byte*>(dev.map_buffer(_hit_group_buffer, 0, 0));
		lgpu::shader_group_handle handle = uninitialized;
		// ray gen
		handle = dev.get_shader_group_handle(_pipeline_state, 3);
		std::memcpy(raygen_rec, handle.data().data(), handle.data().size());
		// miss
		handle = dev.get_shader_group_handle(_pipeline_state, 2);
		std::memcpy(miss_rec, handle.data().data(), handle.data().size());
		// hit groups
		handle = dev.get_shader_group_handle(_pipeline_state, 0);
		std::memcpy(hit_group_rec, handle.data().data(), handle.data().size());
		handle = dev.get_shader_group_handle(_pipeline_state, 1);
		std::memcpy(hit_group_rec + prop.shader_group_handle_alignment, handle.data().data(), handle.data().size());
		// done
		dev.unmap_buffer(_raygen_buffer, 0, _shader_group_handle_size);
		dev.unmap_buffer(_miss_buffer, 0, _shader_group_handle_size);
		dev.unmap_buffer(_hit_group_buffer, 0, _shader_group_handle_size * 2);
	}

	input_resources create_input_resources(lgpu::device &dev, lgpu::descriptor_pool &pool, gltf::Model &raw_scene, scene_resources &scene, cvec2s output_size, lgpu::image_view &output_image, lgpu::sampler &sampler) {
		input_resources result;
		result.constant_buffer = dev.create_committed_buffer(sizeof(global_data), lgpu::heap_type::upload, lgpu::buffer_usage::mask::read_only_buffer);
		result.descriptors = dev.create_descriptor_set(pool, _rt_descriptor_layout);
		dev.write_descriptor_set_acceleration_structures(
			result.descriptors, _rt_descriptor_layout, 0,
			{ &scene.tlas }
		);
		dev.write_descriptor_set_constant_buffers(
			result.descriptors, _rt_descriptor_layout, 1,
			{ lgpu::constant_buffer_view::create(result.constant_buffer, 0, sizeof(global_data)), }
		);
		dev.write_descriptor_set_read_write_images(
			result.descriptors, _rt_descriptor_layout, 2,
			{ &output_image }
		);
		dev.write_descriptor_set_read_only_structured_buffers(
			result.descriptors, _rt_descriptor_layout, 3,
			{
				lgpu::structured_buffer_view::create(scene.material_buffer, 0, raw_scene.materials.size(), sizeof(scene_resources::material_data)),
				lgpu::structured_buffer_view::create(scene.vertex_buffer, 0, scene.vertex_count, sizeof(scene_resources::vertex)),
				lgpu::structured_buffer_view::create(scene.index_buffer, 0, scene.index_count, sizeof(std::uint32_t)),
				lgpu::structured_buffer_view::create(scene.instance_buffer, 0, scene.instances.size(), sizeof(scene_resources::instance_data)),
			}
		);
		dev.write_descriptor_set_samplers(
			result.descriptors, _rt_descriptor_layout, 7, { &sampler }
		);
		result.output_size = output_size;
		return result;
	}

	void record_commands(
		lgpu::command_list &list, const gltf::Model &model,
		scene_resources &model_rsrc, const input_resources &input_rsrc, lgpu::image &out_buffer
	) {
		list.resource_barrier(
			{
				lgpu::image_barrier::create(lgpu::subresource_index::first_color(), out_buffer, lgpu::image_usage::read_only_texture, lgpu::image_usage::read_write_color_texture),
			},
			{
				lgpu::buffer_barrier::create(model_rsrc.vertex_buffer, lgpu::buffer_usage::vertex_buffer, lgpu::buffer_usage::read_only_buffer),
				lgpu::buffer_barrier::create(model_rsrc.index_buffer, lgpu::buffer_usage::index_buffer, lgpu::buffer_usage::read_only_buffer),
			}
		);
		list.bind_pipeline_state(_pipeline_state);
		list.bind_ray_tracing_descriptor_sets(
			_pipeline_resources, 0, { &model_rsrc.textures_descriptor_set, &input_rsrc.descriptors }
		);
		list.trace_rays(
			lgpu::constant_buffer_view::create(_raygen_buffer, 0, _shader_group_handle_size),
			lgpu::shader_record_view::create(_miss_buffer, 0, 1, _shader_group_handle_size),
			lgpu::shader_record_view::create(_hit_group_buffer, 0, 2, _shader_group_handle_size),
			input_rsrc.output_size[0], input_rsrc.output_size[1], 1
		);
		list.resource_barrier(
			{},
			{
				lgpu::buffer_barrier::create(model_rsrc.vertex_buffer, lgpu::buffer_usage::read_only_buffer, lgpu::buffer_usage::vertex_buffer),
				lgpu::buffer_barrier::create(model_rsrc.index_buffer, lgpu::buffer_usage::read_only_buffer, lgpu::buffer_usage::index_buffer),
			}
		);
	}
protected:
	lgpu::shader_binary _shaders = nullptr;
	lgpu::descriptor_set_layout _rt_descriptor_layout = nullptr;
	lgpu::pipeline_resources	_pipeline_resources = nullptr;
	lgpu::raytracing_pipeline_state _pipeline_state = nullptr;
	std::size_t _shader_group_handle_size;
	lgpu::buffer _raygen_buffer = nullptr;
	lgpu::buffer _miss_buffer = nullptr;
	lgpu::buffer _hit_group_buffer = nullptr;
};
