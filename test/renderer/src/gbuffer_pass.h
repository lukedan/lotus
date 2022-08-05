#pragma once

#include "common.h"
#include "shader_types.h"

// base color + metalness: r8g8b8a8_unorm
// normal: r32g32b32_float
// gloss: r8_unorm

struct gbuffer {
	constexpr static lgpu::format base_color_metalness_format = lgpu::format::r8g8b8a8_unorm;
	constexpr static lgpu::format normal_format = lgpu::format::r32g32b32a32_float;
	constexpr static lgpu::format gloss_format = lgpu::format::r8_unorm;
	constexpr static lgpu::format depth_stencil_format = lgpu::format::d32_float_s8;

	gbuffer(std::nullptr_t) {
	}

	ren::image2d_view base_color_metalness = nullptr;
	ren::image2d_view normal = nullptr;
	ren::image2d_view gloss = nullptr;
	ren::image2d_view depth_stencil = nullptr;
};

class gbuffer_pass {
public:
	struct constants {
		constants(uninitialized_t) {
		}

		mat44f view = uninitialized; ///< View matrix.
		mat44f projection_view = uninitialized; ///< Projection matrix times view matrix.
	};
	struct input_resources {
		lgpu::buffer constant_buffer = nullptr;
		lgpu::descriptor_set constant_descriptor_set = nullptr;
	};
	struct output_resources {
		lgpu::frame_buffer frame_buffer = nullptr;
		cvec2s viewport_size = uninitialized;
	};

	gbuffer_pass(lgpu::device &dev, const lgpu::descriptor_set_layout &textures_layout, const lgpu::descriptor_set_layout &mat_set_layout, const lgpu::descriptor_set_layout &node_set_layout) :
		_constant_descriptors_layout(dev.create_descriptor_set_layout(
			{
				lgpu::descriptor_range_binding::create(lgpu::descriptor_range::create(lgpu::descriptor_type::constant_buffer, 1), 0),
				lgpu::descriptor_range_binding::create(lgpu::descriptor_type::sampler, 1, 1),
			},
			lgpu::shader_stage::all
		)),
		_pipeline_resources(dev.create_pipeline_resources(
			{ &textures_layout, &mat_set_layout, &node_set_layout, &_constant_descriptors_layout }
		)),
		_pass_resources(dev.create_pass_resources(
			{
				lgpu::render_target_pass_options::create(gbuffer::base_color_metalness_format, lgpu::pass_load_operation::discard, lgpu::pass_store_operation::preserve),
				lgpu::render_target_pass_options::create(gbuffer::normal_format, lgpu::pass_load_operation::discard, lgpu::pass_store_operation::preserve),
				lgpu::render_target_pass_options::create(gbuffer::gloss_format, lgpu::pass_load_operation::discard, lgpu::pass_store_operation::preserve),
			},
			lgpu::depth_stencil_pass_options::create(
				gbuffer::depth_stencil_format,
				lgpu::pass_load_operation::clear, lgpu::pass_store_operation::preserve,
				lgpu::pass_load_operation::discard, lgpu::pass_store_operation::discard
			)
		)) {

		_vs_binary = load_binary_file("shaders/gbuffer.vs.o");
		_ps_binary = load_binary_file("shaders/gbuffer.ps.o");
		_vertex_shader = dev.load_shader(_vs_binary);
		_pixel_shader = dev.load_shader(_ps_binary);

		auto shaders = lgpu::shader_set::create(_vertex_shader, _pixel_shader);
		auto rasterizer = lgpu::rasterizer_options::create(lgpu::depth_bias_options::disabled(), lgpu::front_facing_mode::counter_clockwise, lgpu::cull_mode::none);
		auto depth_stencil = lgpu::depth_stencil_options::create(
			true, true, lgpu::comparison_function::greater,
			false, 0, 0, lgpu::stencil_options::always_pass_no_op(), lgpu::stencil_options::always_pass_no_op()
		);
		std::vector<lgpu::input_buffer_element> vert_buffer_elements{
			lgpu::input_buffer_element::create(u8"POSITION", 0, lgpu::format::r32g32b32_float, offsetof(scene_resources::vertex, position)),
			lgpu::input_buffer_element::create(u8"NORMAL", 0, lgpu::format::r32g32b32_float, offsetof(scene_resources::vertex, normal)),
			lgpu::input_buffer_element::create(u8"TANGENT", 0, lgpu::format::r32g32b32a32_float, offsetof(scene_resources::vertex, tangent)),
			lgpu::input_buffer_element::create(u8"TEXCOORD", 0, lgpu::format::r32g32_float, offsetof(scene_resources::vertex, uv)),
		};
		_pipeline_state = dev.create_graphics_pipeline_state(
			_pipeline_resources, shaders,
			{
				lgpu::render_target_blend_options::disabled(),
				lgpu::render_target_blend_options::disabled(),
				lgpu::render_target_blend_options::disabled(),
			},
			rasterizer, depth_stencil,
			{ lgpu::input_buffer_layout::create_vertex_buffer<scene_resources::vertex>(vert_buffer_elements, 0) },
			lgpu::primitive_topology::triangle_list, _pass_resources
		);
	}

	void record_commands(
		lgpu::command_list &list, gbuffer &gbuf, const gltf::Model &model,
		scene_resources &model_rsrc, const input_resources &input_rsrc, const output_resources &output_rsrc
	) {
		list.resource_barrier(
			{
				lgpu::image_barrier::create(lgpu::subresource_index::first_color(), gbuf.base_color_metalness, lgpu::image_usage::read_only_texture, lgpu::image_usage::color_render_target),
				lgpu::image_barrier::create(lgpu::subresource_index::first_color(), gbuf.normal, lgpu::image_usage::read_only_texture, lgpu::image_usage::color_render_target),
				lgpu::image_barrier::create(lgpu::subresource_index::first_color(), gbuf.gloss, lgpu::image_usage::read_only_texture, lgpu::image_usage::color_render_target),
				lgpu::image_barrier::create(lgpu::subresource_index::first_depth_stencil(), gbuf.depth_stencil, lgpu::image_usage::read_only_texture, lgpu::image_usage::depth_stencil_render_target),
			},
			{}
		);

		list.begin_pass(
			_pass_resources, output_rsrc.frame_buffer,
			{
				linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f),
				linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f),
				linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f)
			},
			0.0f, 0
		);

		auto viewport = lgpu::viewport::create(aab2f::create_from_min_max(zero, output_rsrc.viewport_size.into<float>()), 0.0f, 1.0f);
		auto scissor = aab2i::create_from_min_max(zero, output_rsrc.viewport_size.into<int>());
		list.set_viewports({ viewport });
		list.set_scissor_rectangles({ scissor });

		for (std::size_t node_i = 0; node_i < model.nodes.size(); ++node_i) {
			const auto &node = model.nodes[node_i];
			if (node.mesh < 0) {
				continue;
			}
			const auto &mesh = model.meshes[node.mesh];
			for (std::size_t prim_i = 0; prim_i < mesh.primitives.size(); ++prim_i) {
				const auto &prim = mesh.primitives[prim_i];
				const auto &instance = model_rsrc.instances[model_rsrc.instance_indices[node.mesh][prim_i]];

				std::vector<lgpu::vertex_buffer> vert_buffers{
					lgpu::vertex_buffer::from_buffer_offset_stride(
						model_rsrc.vertex_buffer, sizeof(scene_resources::vertex) * instance.first_vertex, sizeof(scene_resources::vertex)
					)
				};

				list.bind_pipeline_state(_pipeline_state);
				list.bind_vertex_buffers(0, vert_buffers);
				list.bind_graphics_descriptor_sets(
					_pipeline_resources, 0,
					{
						&model_rsrc.textures_descriptor_set,
						&model_rsrc.material_descriptor_sets[prim.material],
						&model_rsrc.node_descriptor_sets[node_i],
						&input_rsrc.constant_descriptor_set
					}
				);

				if (prim.indices < 0) {
					list.draw_instanced(0, model.accessors[prim.attributes.begin()->second].count, node_i, 1);
				} else {
					const auto &index_accessor = model.accessors[prim.indices];
					list.bind_index_buffer(model_rsrc.index_buffer, sizeof(std::uint32_t) * instance.first_index, lgpu::index_format::uint32);
					list.draw_indexed_instanced(0, index_accessor.count, 0, 0, 1);
				}
			}
		}

		list.end_pass();

		list.resource_barrier(
			{
				lgpu::image_barrier::create(lgpu::subresource_index::first_color(), gbuf.base_color_metalness, lgpu::image_usage::color_render_target, lgpu::image_usage::read_only_texture),
				lgpu::image_barrier::create(lgpu::subresource_index::first_color(), gbuf.normal, lgpu::image_usage::color_render_target, lgpu::image_usage::read_only_texture),
				lgpu::image_barrier::create(lgpu::subresource_index::first_color(), gbuf.gloss, lgpu::image_usage::color_render_target, lgpu::image_usage::read_only_texture),
				lgpu::image_barrier::create(lgpu::subresource_index::first_depth_stencil(), gbuf.depth_stencil, lgpu::image_usage::depth_stencil_render_target, lgpu::image_usage::read_only_texture),
			},
			{}
		);
	}

	[[nodiscard]] input_resources create_input_resources(
		lgpu::device &dev, const lgpu::adapter_properties &props, lgpu::descriptor_pool &pool, lgpu::sampler &sampler, const gltf::Model &model, const scene_resources &rsrc
	) const {
		input_resources result;

		std::size_t aligned_global_data_size = memory::align_size(sizeof(constants), props.constant_buffer_alignment);
		result.constant_buffer = dev.create_committed_buffer(
			aligned_global_data_size, lgpu::heap_type::upload, lgpu::buffer_usage::mask::read_only_buffer
		);

		result.constant_descriptor_set = dev.create_descriptor_set(pool, _constant_descriptors_layout);
		dev.write_descriptor_set_constant_buffers(
			result.constant_descriptor_set, _constant_descriptors_layout, 0,
			{
				lgpu::constant_buffer_view::create(result.constant_buffer, 0, sizeof(constants)),
			}
		);
		dev.write_descriptor_set_samplers(
			result.constant_descriptor_set, _constant_descriptors_layout, 1, { &sampler }
		);


		return result;
	}

	[[nodiscard]] output_resources create_output_resources(
		lgpu::device &dev, const gbuffer::view &gbuf, cvec2s viewport_size
	) const {
		output_resources result;
		result.frame_buffer = dev.create_frame_buffer({ &gbuf.base_color_metalness, &gbuf.normal, &gbuf.gloss }, &gbuf.depth_stencil, viewport_size, _pass_resources);
		result.viewport_size = viewport_size;
		return result;
	}
protected:
	std::vector<std::byte> _vs_binary;
	std::vector<std::byte> _ps_binary;
	lgpu::shader_binary _vertex_shader = nullptr;
	lgpu::shader_binary _pixel_shader = nullptr;
	lgpu::descriptor_set_layout _constant_descriptors_layout;
	lgpu::pipeline_resources _pipeline_resources;
	lgpu::graphics_pipeline_state _pipeline_state = nullptr;
	lgpu::pass_resources _pass_resources;
};
