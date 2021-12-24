#pragma once

#include "common.h"

// base color + metalness: r8g8b8a8_unorm
// normal: r32g32b32_float
// gloss: r8_unorm

struct gbuffer {
	constexpr static gfx::format base_color_metalness_format = gfx::format::r8g8b8a8_unorm;
	constexpr static gfx::format normal_format = gfx::format::r32g32b32a32_float;
	constexpr static gfx::format gloss_format = gfx::format::r8_unorm;
	constexpr static gfx::format depth_stencil_format = gfx::format::d32_float_s8;

	struct view {
		gfx::image2d_view base_color_metalness = nullptr;
		gfx::image2d_view normal = nullptr;
		gfx::image2d_view gloss = nullptr;
		gfx::image2d_view depth_stencil = nullptr;
	};

	gfx::image2d base_color_metalness = nullptr;
	gfx::image2d normal = nullptr;
	gfx::image2d gloss = nullptr;
	gfx::image2d depth_stencil = nullptr;

	[[nodiscard]] inline static gbuffer create(gfx::device &dev, gfx::command_allocator &alloc, gfx::command_queue &q, cvec2s size) {
		gbuffer result;
		result.base_color_metalness = dev.create_committed_image2d(
			size[0], size[1], 1, 1, base_color_metalness_format, gfx::image_tiling::optimal,
			gfx::image_usage::mask::color_render_target | gfx::image_usage::mask::read_only_texture
		);
		dev.set_debug_name(result.base_color_metalness, u8"Base color + Metalness");
		result.normal = dev.create_committed_image2d(
			size[0], size[1], 1, 1, normal_format, gfx::image_tiling::optimal,
			gfx::image_usage::mask::color_render_target | gfx::image_usage::mask::read_only_texture
		);
		dev.set_debug_name(result.normal, u8"Normal");
		result.gloss = dev.create_committed_image2d(
			size[0], size[1], 1, 1, gloss_format, gfx::image_tiling::optimal,
			gfx::image_usage::mask::color_render_target | gfx::image_usage::mask::read_only_texture
		);
		dev.set_debug_name(result.gloss, u8"Gloss");
		result.depth_stencil = dev.create_committed_image2d(
			size[0], size[1], 1, 1, depth_stencil_format, gfx::image_tiling::optimal,
			gfx::image_usage::mask::depth_stencil_render_target | gfx::image_usage::mask::read_only_texture
		);

		auto list = dev.create_and_start_command_list(alloc);
		list.resource_barrier(
			{
				gfx::image_barrier::create(gfx::subresource_index::first_color(), result.base_color_metalness, gfx::image_usage::initial, gfx::image_usage::read_only_texture),
				gfx::image_barrier::create(gfx::subresource_index::first_color(), result.normal, gfx::image_usage::initial, gfx::image_usage::read_only_texture),
				gfx::image_barrier::create(gfx::subresource_index::first_color(), result.gloss, gfx::image_usage::initial, gfx::image_usage::read_only_texture),
				gfx::image_barrier::create(gfx::subresource_index::first_depth_stencil(), result.depth_stencil, gfx::image_usage::initial, gfx::image_usage::read_only_texture),
			},
			{}
		);
		list.finish();
		auto fence = dev.create_fence(gfx::synchronization_state::unset);
		q.submit_command_lists({ &list }, &fence);
		dev.wait_for_fence(fence);

		return result;
	}

	[[nodiscard]] view create_view(gfx::device &dev) const {
		view result;
		result.base_color_metalness = dev.create_image2d_view_from(base_color_metalness, base_color_metalness_format, gfx::mip_levels::only_highest());
		result.normal = dev.create_image2d_view_from(normal, normal_format, gfx::mip_levels::only_highest());
		result.gloss = dev.create_image2d_view_from(gloss, gloss_format, gfx::mip_levels::only_highest());
		result.depth_stencil = dev.create_image2d_view_from(depth_stencil, depth_stencil_format, gfx::mip_levels::only_highest());
		return result;
	}
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
		gfx::buffer constant_buffer = nullptr;
		gfx::descriptor_set constant_descriptor_set = nullptr;
	};
	struct output_resources {
		gfx::frame_buffer frame_buffer = nullptr;
		cvec2s viewport_size = uninitialized;
	};

	gbuffer_pass(gfx::device &dev, const gfx::descriptor_set_layout &textures_layout, const gfx::descriptor_set_layout &mat_set_layout, const gfx::descriptor_set_layout &node_set_layout) :
		_constant_descriptors_layout(dev.create_descriptor_set_layout(
			{
				gfx::descriptor_range_binding::create(gfx::descriptor_range::create(gfx::descriptor_type::constant_buffer, 1), 0),
				gfx::descriptor_range_binding::create(gfx::descriptor_type::sampler, 1, 1),
			},
			gfx::shader_stage::all
		)),
		_pipeline_resources(dev.create_pipeline_resources(
			{ &textures_layout, &mat_set_layout, &node_set_layout, &_constant_descriptors_layout }
		)),
		_pass_resources(dev.create_pass_resources(
			{
				gfx::render_target_pass_options::create(gbuffer::base_color_metalness_format, gfx::pass_load_operation::discard, gfx::pass_store_operation::preserve),
				gfx::render_target_pass_options::create(gbuffer::normal_format, gfx::pass_load_operation::discard, gfx::pass_store_operation::preserve),
				gfx::render_target_pass_options::create(gbuffer::gloss_format, gfx::pass_load_operation::discard, gfx::pass_store_operation::preserve),
			},
			gfx::depth_stencil_pass_options::create(
				gbuffer::depth_stencil_format,
				gfx::pass_load_operation::clear, gfx::pass_store_operation::preserve,
				gfx::pass_load_operation::discard, gfx::pass_store_operation::discard
			)
		)) {

		_vs_binary = load_binary_file("shaders/gbuffer.vs.o");
		_ps_binary = load_binary_file("shaders/gbuffer.ps.o");
		_vertex_shader = dev.load_shader(_vs_binary);
		_pixel_shader = dev.load_shader(_ps_binary);

		auto shaders = gfx::shader_set::create(_vertex_shader, _pixel_shader);
		auto rasterizer = gfx::rasterizer_options::create(gfx::depth_bias_options::disabled(), gfx::front_facing_mode::counter_clockwise, gfx::cull_mode::none);
		auto depth_stencil = gfx::depth_stencil_options::create(
			true, true, gfx::comparison_function::greater,
			false, 0, 0, gfx::stencil_options::always_pass_no_op(), gfx::stencil_options::always_pass_no_op()
		);
		std::vector<gfx::input_buffer_element> vert_buffer_elements{
			gfx::input_buffer_element::create(u8"POSITION", 0, gfx::format::r32g32b32_float, offsetof(scene_resources::vertex, position)),
			gfx::input_buffer_element::create(u8"NORMAL", 0, gfx::format::r32g32b32_float, offsetof(scene_resources::vertex, normal)),
			gfx::input_buffer_element::create(u8"TANGENT", 0, gfx::format::r32g32b32a32_float, offsetof(scene_resources::vertex, tangent)),
			gfx::input_buffer_element::create(u8"TEXCOORD", 0, gfx::format::r32g32_float, offsetof(scene_resources::vertex, uv)),
		};
		_pipeline_state = dev.create_graphics_pipeline_state(
			_pipeline_resources, shaders,
			{
				gfx::render_target_blend_options::disabled(),
				gfx::render_target_blend_options::disabled(),
				gfx::render_target_blend_options::disabled(),
			},
			rasterizer, depth_stencil,
			{ gfx::input_buffer_layout::create_vertex_buffer<scene_resources::vertex>(vert_buffer_elements, 0) },
			gfx::primitive_topology::triangle_list, _pass_resources
		);
	}

	void record_commands(
		gfx::command_list &list, gbuffer &gbuf, const gltf::Model &model,
		scene_resources &model_rsrc, const input_resources &input_rsrc, const output_resources &output_rsrc
	) {
		list.resource_barrier(
			{
				gfx::image_barrier::create(gfx::subresource_index::first_color(), gbuf.base_color_metalness, gfx::image_usage::read_only_texture, gfx::image_usage::color_render_target),
				gfx::image_barrier::create(gfx::subresource_index::first_color(), gbuf.normal, gfx::image_usage::read_only_texture, gfx::image_usage::color_render_target),
				gfx::image_barrier::create(gfx::subresource_index::first_color(), gbuf.gloss, gfx::image_usage::read_only_texture, gfx::image_usage::color_render_target),
				gfx::image_barrier::create(gfx::subresource_index::first_depth_stencil(), gbuf.depth_stencil, gfx::image_usage::read_only_texture, gfx::image_usage::depth_stencil_render_target),
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

		auto viewport = gfx::viewport::create(aab2f::create_from_min_max(zero, output_rsrc.viewport_size.into<float>()), 0.0f, 1.0f);
		auto scissor = aab2i::create_from_min_max(zero, output_rsrc.viewport_size.into<int>());
		list.set_viewports({ viewport });
		list.set_scissor_rectangles({ scissor });

		for (std::size_t node_i = 0; node_i < model.nodes.size(); ++node_i) {
			const auto &node = model.nodes[node_i];
			const auto &mesh = model.meshes[node.mesh];
			for (std::size_t prim_i = 0; prim_i < mesh.primitives.size(); ++prim_i) {
				const auto &prim = mesh.primitives[prim_i];
				const auto &instance = model_rsrc.instances[model_rsrc.instance_indices[node.mesh][prim_i]];

				std::vector<gfx::vertex_buffer> vert_buffers{
					gfx::vertex_buffer::from_buffer_offset_stride(
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
					list.bind_index_buffer(model_rsrc.index_buffer, sizeof(std::uint32_t) * instance.first_index, gfx::index_format::uint32);
					list.draw_indexed_instanced(0, index_accessor.count, 0, 0, 1);
				}
			}
		}

		list.end_pass();

		list.resource_barrier(
			{
				gfx::image_barrier::create(gfx::subresource_index::first_color(), gbuf.base_color_metalness, gfx::image_usage::color_render_target, gfx::image_usage::read_only_texture),
				gfx::image_barrier::create(gfx::subresource_index::first_color(), gbuf.normal, gfx::image_usage::color_render_target, gfx::image_usage::read_only_texture),
				gfx::image_barrier::create(gfx::subresource_index::first_color(), gbuf.gloss, gfx::image_usage::color_render_target, gfx::image_usage::read_only_texture),
				gfx::image_barrier::create(gfx::subresource_index::first_depth_stencil(), gbuf.depth_stencil, gfx::image_usage::depth_stencil_render_target, gfx::image_usage::read_only_texture),
			},
			{}
		);
	}

	[[nodiscard]] input_resources create_input_resources(
		gfx::device &dev, const gfx::adapter_properties &props, gfx::descriptor_pool &pool, gfx::sampler &sampler, const gltf::Model &model, const scene_resources &rsrc
	) const {
		input_resources result;

		std::size_t aligned_global_data_size = align_size(sizeof(constants), props.constant_buffer_alignment);
		result.constant_buffer = dev.create_committed_buffer(
			aligned_global_data_size, gfx::heap_type::upload, gfx::buffer_usage::mask::read_only_buffer
		);

		result.constant_descriptor_set = dev.create_descriptor_set(pool, _constant_descriptors_layout);
		dev.write_descriptor_set_constant_buffers(
			result.constant_descriptor_set, _constant_descriptors_layout, 0,
			{
				gfx::constant_buffer_view::create(result.constant_buffer, 0, sizeof(constants)),
			}
		);
		dev.write_descriptor_set_samplers(
			result.constant_descriptor_set, _constant_descriptors_layout, 1, { &sampler }
		);


		return result;
	}

	[[nodiscard]] output_resources create_output_resources(
		gfx::device &dev, const gbuffer::view &gbuf, cvec2s viewport_size
	) const {
		output_resources result;
		result.frame_buffer = dev.create_frame_buffer({ &gbuf.base_color_metalness, &gbuf.normal, &gbuf.gloss }, &gbuf.depth_stencil, viewport_size, _pass_resources);
		result.viewport_size = viewport_size;
		return result;
	}
protected:
	std::vector<std::byte> _vs_binary;
	std::vector<std::byte> _ps_binary;
	gfx::shader_binary _vertex_shader = nullptr;
	gfx::shader_binary _pixel_shader = nullptr;
	gfx::descriptor_set_layout _constant_descriptors_layout;
	gfx::pipeline_resources _pipeline_resources;
	gfx::graphics_pipeline_state _pipeline_state = nullptr;
	gfx::pass_resources _pass_resources;
};
