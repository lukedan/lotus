#pragma once

#include "gbuffer_pass.h"

class raytrace_resolve_pass {
public:
	struct global_data {
		global_data(uninitialized_t) {
		}

		std::uint32_t frame_index;
	};

	struct input_resources {
		gfx::buffer globals_buffer = nullptr;
		gfx::descriptor_set descriptor_set = nullptr;
	};
	struct output_resources {
		gfx::image2d_view image_view = nullptr;
		gfx::frame_buffer frame_buffer = nullptr;
		cvec2s viewport_size = uninitialized;

		gfx::pass_resources pass_resources = nullptr;
		gfx::graphics_pipeline_state pipeline_state = nullptr;
	};

	raytrace_resolve_pass(gfx::device &dev) :
		_point_sampler(dev.create_sampler(
			gfx::filtering::nearest, gfx::filtering::nearest, gfx::filtering::nearest,
			0, 0, 0, std::nullopt,
			gfx::sampler_address_mode::border, gfx::sampler_address_mode::border, gfx::sampler_address_mode::border,
			linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f), std::nullopt
		)),
		_gbuffer_descriptors_layout(dev.create_descriptor_set_layout(
			{
				gfx::descriptor_range_binding::create(gfx::descriptor_type::read_only_image, 1, 0),
				gfx::descriptor_range_binding::create(gfx::descriptor_type::sampler, 1, 1),
				gfx::descriptor_range_binding::create(gfx::descriptor_type::constant_buffer, 1, 2),
			},
			gfx::shader_stage::all
		)),
		_pipeline_resources(dev.create_pipeline_resources({ &_gbuffer_descriptors_layout })) {

		auto vs_binary = load_binary_file("shaders/rt_resolve.vs.o");
		auto ps_binary = load_binary_file("shaders/rt_resolve.ps.o");
		_vertex_shader = dev.load_shader(vs_binary);
		_pixel_shader = dev.load_shader(ps_binary);
	}

	void record_commands(
		gfx::command_list &list, gfx::image &img, gfx::image &input, const input_resources &input_rsrc, const output_resources &output_rsrc
	) {
		list.resource_barrier(
			{
				gfx::image_barrier::create(gfx::subresource_index::first_color(), img, gfx::image_usage::present, gfx::image_usage::color_render_target),
				gfx::image_barrier::create(gfx::subresource_index::first_color(), input, gfx::image_usage::read_write_color_texture, gfx::image_usage::read_only_texture),
			},
			{}
		);

		list.set_viewports({ gfx::viewport::create(aab2f::create_from_min_max(zero, output_rsrc.viewport_size.into<float>()), 0.0f, 1.0f) });
		list.set_scissor_rectangles({ aab2i::create_from_min_max(zero, output_rsrc.viewport_size.into<int>()) });

		list.begin_pass(output_rsrc.pass_resources, output_rsrc.frame_buffer, { linear_rgba_f(0.0f, 0.0f, 0.0f, 0.0f) }, 0.0f, 0);
		list.bind_pipeline_state(output_rsrc.pipeline_state);
		list.bind_graphics_descriptor_sets(_pipeline_resources, 0, { &input_rsrc.descriptor_set });
		list.draw_instanced(0, 3, 0, 1);
		list.end_pass();

		list.resource_barrier(
			{
				gfx::image_barrier::create(gfx::subresource_index::first_color(), img, gfx::image_usage::color_render_target, gfx::image_usage::present),
			},
			{}
		);
	}

	[[nodiscard]] input_resources create_input_resources(gfx::device &dev, gfx::descriptor_pool &pool, gfx::image2d_view &input) const {
		input_resources result;
		result.globals_buffer = dev.create_committed_buffer(sizeof(global_data), gfx::heap_type::upload, gfx::buffer_usage::mask::read_only_buffer);
		result.descriptor_set = dev.create_descriptor_set(pool, _gbuffer_descriptors_layout);
		dev.write_descriptor_set_read_only_images(
			result.descriptor_set, _gbuffer_descriptors_layout, 0, { &input }
		);
		dev.write_descriptor_set_samplers(
			result.descriptor_set, _gbuffer_descriptors_layout, 1, { &_point_sampler }
		);
		dev.write_descriptor_set_constant_buffers(
			result.descriptor_set, _gbuffer_descriptors_layout, 2, { gfx::constant_buffer_view::create(result.globals_buffer, 0, sizeof(global_data)), }
		);
		return result;
	}
	[[nodiscard]] output_resources create_output_resources(gfx::device &dev, gfx::image2d &img, gfx::format fmt, cvec2s size) const {
		output_resources result;

		result.pass_resources = dev.create_pass_resources(
			{
				gfx::render_target_pass_options::create(fmt, gfx::pass_load_operation::preserve, gfx::pass_store_operation::preserve),
			},
			gfx::depth_stencil_pass_options::create(
				gfx::format::none,
				gfx::pass_load_operation::discard, gfx::pass_store_operation::discard,
				gfx::pass_load_operation::discard, gfx::pass_store_operation::discard
			)
		);
		result.pipeline_state = dev.create_graphics_pipeline_state(
			_pipeline_resources,
			gfx::shader_set::create(_vertex_shader, _pixel_shader),
			{ gfx::render_target_blend_options::disabled() },
			gfx::rasterizer_options::create(gfx::depth_bias_options::create_unclamped(0.0f, 0.0f), gfx::front_facing_mode::clockwise, gfx::cull_mode::none),
			gfx::depth_stencil_options::create(false, false, gfx::comparison_function::always, false, 0, 0, gfx::stencil_options::always_pass_no_op(), gfx::stencil_options::always_pass_no_op()),
			{},
			gfx::primitive_topology::triangle_strip,
			result.pass_resources,
			1
		);

		result.image_view = dev.create_image2d_view_from(img, fmt, gfx::mip_levels::only_highest());
		result.frame_buffer = dev.create_frame_buffer({ &result.image_view }, nullptr, size, result.pass_resources);
		result.viewport_size = size;

		return result;
	}
protected:
	gfx::shader_binary _vertex_shader = nullptr;
	gfx::shader_binary _pixel_shader = nullptr;
	gfx::sampler _point_sampler;
	gfx::descriptor_set_layout _gbuffer_descriptors_layout;
	gfx::pipeline_resources _pipeline_resources;
};
