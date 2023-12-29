#include "lotus/renderer/g_buffer.h"

/// \file
/// Implementation of the G-buffer.

#include "lotus/renderer/context/asset_manager.h"
#include "lotus/renderer/shader_types.h"

namespace lotus::renderer::g_buffer {
	view view::create(context &ctx, cvec2u32 size, const pool &p) {
		view result = nullptr;
		result.albedo_glossiness = ctx.request_image2d(u8"GBuffer Albedo-glossiness", size, 1, albedo_glossiness_format, gpu::image_usage_mask::color_render_target | gpu::image_usage_mask::shader_read, p);
		result.normal            = ctx.request_image2d(u8"GBuffer Normal",            size, 1, normal_format,            gpu::image_usage_mask::color_render_target | gpu::image_usage_mask::shader_read, p);
		result.metalness         = ctx.request_image2d(u8"GBuffer Metalness",         size, 1, metalness_format,         gpu::image_usage_mask::color_render_target | gpu::image_usage_mask::shader_read, p);
		result.velocity          = ctx.request_image2d(u8"GBuffer Velocity",          size, 1, velocity_format,          gpu::image_usage_mask::color_render_target | gpu::image_usage_mask::shader_read, p);
		result.depth_stencil     = ctx.request_image2d(u8"GBuffer Depth-stencil",     size, 1, depth_stencil_format,     gpu::image_usage_mask::depth_stencil_render_target | gpu::image_usage_mask::shader_read, p);
		return result;
	}

	context::pass view::begin_pass(context::queue &q) {
		return q.begin_pass(
			{
				image2d_color(albedo_glossiness, gpu::color_render_target_access::create_discard_then_write()),
				image2d_color(normal,            gpu::color_render_target_access::create_discard_then_write()),
				image2d_color(metalness,         gpu::color_render_target_access::create_discard_then_write()),
				image2d_color(velocity,          gpu::color_render_target_access::create_discard_then_write()),
			},
			image2d_depth_stencil(
				depth_stencil,
				gpu::depth_render_target_access::create_clear(0.0f),
				gpu::stencil_render_target_access::create_clear(0)
			),
			depth_stencil.get_size(),
			u8"G-Buffer pass"
		);
	}


	instance_render_details pass_context::get_render_details(
		context&, const assets::material::context_data &mat_ctx, const assets::geometry &geom
	) {
		instance_render_details details = nullptr;

		std::vector<std::pair<std::u8string_view, std::u8string_view>> vs_defines = {
			{ u8"LOTUS_MATERIAL_INCLUDE", mat_ctx.get_material_include() },
		};
		auto ps_defines = mat_ctx.get_additional_ps_defines();
		ps_defines.emplace_back(u8"LOTUS_MATERIAL_INCLUDE", mat_ctx.get_material_include());

		if (geom.vertex_buffer.data) {
			details.input_buffers.emplace_back(geom.vertex_buffer.into_input_buffer_binding(
				u8"POSITION", 0, static_cast<std::uint32_t>(details.input_buffers.size())
			));
		}
		if (geom.uv_buffer.data) {
			details.input_buffers.emplace_back(geom.uv_buffer.into_input_buffer_binding(
				u8"TEXCOORD", 0, static_cast<std::uint32_t>(details.input_buffers.size())
			));
			vs_defines.emplace_back(u8"VERTEX_INPUT_HAS_UV", u8"");
		}
		if (geom.normal_buffer.data) {
			details.input_buffers.emplace_back(geom.normal_buffer.into_input_buffer_binding(
				u8"NORMAL", 0, static_cast<std::uint32_t>(details.input_buffers.size())
			));
			vs_defines.emplace_back(u8"VERTEX_INPUT_HAS_NORMAL", u8"");
		}
		if (geom.tangent_buffer.data) {
			details.input_buffers.emplace_back(geom.tangent_buffer.into_input_buffer_binding(
				u8"TANGENT", 0, static_cast<std::uint32_t>(details.input_buffers.size())
			));
			vs_defines.emplace_back(u8"VERTEX_INPUT_HAS_TANGENT", u8"");
		}

		details.vertex_shader = _man.compile_shader_in_filesystem(
			_man.asset_library_path / "shaders/standard_vertex_shader.hlsl",
			gpu::shader_stage::vertex_shader, u8"main_vs", vs_defines
		);
		details.pixel_shader = _man.compile_shader_in_filesystem(
			_man.asset_library_path / "shaders/gbuffer_pixel_shader.hlsl",
			gpu::shader_stage::pixel_shader, u8"main_ps", ps_defines
		);

		details.pipeline = graphics_pipeline_state(
			{
				gpu::render_target_blend_options::disabled(),
				gpu::render_target_blend_options::disabled(),
				gpu::render_target_blend_options::disabled(),
				gpu::render_target_blend_options::disabled(),
			},
			gpu::rasterizer_options(gpu::depth_bias_options::disabled(), gpu::front_facing_mode::counter_clockwise, gpu::cull_mode::cull_back, false),
			gpu::depth_stencil_options(true, true, gpu::comparison_function::greater, false, 0, 0, gpu::stencil_options::always_pass_no_op(), gpu::stencil_options::always_pass_no_op())
		);

		return details;
	}


	std::vector<instance_render_details> get_instance_render_details(
		assets::manager &man, std::span<const instance> instances
	) {
		pass_context pass_ctx(man);
		std::vector<instance_render_details> result;
		result.reserve(instances.size());
		for (const auto &inst : instances) {
			result.emplace_back(pass_ctx.get_render_details(
				man.get_context(), *inst.material->data, inst.geometry.get().value
			));
		}
		return result;
	}

	void render_instances(
		context::pass &pass, constant_uploader &uploader,
		std::span<const instance> instances, std::span<const instance_render_details> details,
		descriptor_resource::constant_buffer view_data
	) {
		crash_if(instances.size() != details.size());

		for (std::size_t i = 0; i < instances.size(); ++i) {
			const auto &inst = instances[i];
			const auto &dets = details[i];

			if (!inst.material) {
				continue;
			}

			auto normal_trans_inv = mat::lup_decompose(inst.transform.block<3, 3>(0, 0));
			mat44f normal_trans = zero;
			normal_trans.set_block(
				0, 0, normal_trans_inv.invert().transposed() * std::pow(normal_trans_inv.determinant(), 2.0f / 3.0f)
			);
			shader_types::instance_data instance;
			instance.transform        = inst.transform;
			instance.normal_transform = normal_trans;
			instance.prev_transform   = inst.prev_transform;

			auto additional_resources = all_resource_bindings(
				{
					{ 1, {
						{ 1, uploader.upload(instance) },
						{ 2, view_data },
					} },
				},
				{}
			);

			pass.draw_instanced(
				inst.geometry, inst.material, dets,
				{}, std::move(additional_resources), uploader, 1, u8"GBuffer instance"
			);
		}
	}

	void render_instances(
		context::pass &pass, constant_uploader &uploader,
		assets::manager &man, std::span<const instance> instances,
		descriptor_resource::constant_buffer view_data
	) {
		auto details = get_instance_render_details(man, instances);
		render_instances(pass, uploader, instances, details, view_data);
	}
}
