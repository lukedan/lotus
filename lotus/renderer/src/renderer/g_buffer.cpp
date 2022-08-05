#include "lotus/renderer/g_buffer.h"

/// \file
/// Implementation of the G-buffer.

#include "lotus/renderer/asset_manager.h"
#include "lotus/renderer/shader_types.h"

namespace lotus::renderer::g_buffer {
	view view::create(context &ctx, cvec2s size) {
		view result = nullptr;
		result.albedo_glossiness = ctx.request_image2d(u8"GBuffer Albedo-glossiness", size, 1, albedo_glossiness_format, graphics::image_usage::mask::color_render_target | graphics::image_usage::mask::read_only_texture);
		result.normal            = ctx.request_image2d(u8"GBuffer Normal",            size, 1, normal_format,            graphics::image_usage::mask::color_render_target | graphics::image_usage::mask::read_only_texture);
		result.metalness         = ctx.request_image2d(u8"GBuffer Metalness",         size, 1, metalness_format,         graphics::image_usage::mask::color_render_target | graphics::image_usage::mask::read_only_texture);
		result.depth_stencil     = ctx.request_image2d(u8"GBuffer Depth-stencil",     size, 1, depth_stencil_format,     graphics::image_usage::mask::depth_stencil_render_target | graphics::image_usage::mask::read_only_texture);
		return result;
	}

	context::pass view::begin_pass(context &ctx) {
		return ctx.begin_pass(
			{
				surface2d_color(albedo_glossiness, graphics::color_render_target_access::create_discard_then_write()),
				surface2d_color(normal,            graphics::color_render_target_access::create_discard_then_write()),
				surface2d_color(metalness,         graphics::color_render_target_access::create_discard_then_write()),
			},
			surface2d_depth_stencil(
				depth_stencil,
				graphics::depth_render_target_access::create_clear(0.0f),
				graphics::stencil_render_target_access::create_clear(0)
			),
			depth_stencil.get_size(),
			u8"G-Buffer pass"
		);
	}


	std::pair<assets::handle<assets::shader>, std::vector<input_buffer_binding>> pass_context::get_vertex_shader(
		context &ctx, const assets::material::context_data &mat_ctx, const assets::geometry &geom
	) {
		std::vector<std::pair<std::u8string_view, std::u8string_view>> defines = {
			{ u8"LOTUS_MATERIAL_INCLUDE", mat_ctx.get_material_include() },
		};
		std::vector<input_buffer_binding> inputs;
		if (geom.vertex_buffer.data) {
			inputs.emplace_back(geom.vertex_buffer.into_input_buffer_binding(
				u8"POSITION", 0, static_cast<std::uint32_t>(inputs.size())
			));
		}
		if (geom.uv_buffer.data) {
			inputs.emplace_back(geom.uv_buffer.into_input_buffer_binding(
				u8"TEXCOORD", 0, static_cast<std::uint32_t>(inputs.size())
			));
			defines.emplace_back(u8"VERTEX_INPUT_HAS_UV", u8"");
		}
		if (geom.normal_buffer.data) {
			inputs.emplace_back(geom.normal_buffer.into_input_buffer_binding(
				u8"NORMAL", 0, static_cast<std::uint32_t>(inputs.size())
			));
			defines.emplace_back(u8"VERTEX_INPUT_HAS_NORMAL", u8"");
		}
		if (geom.tangent_buffer.data) {
			inputs.emplace_back(geom.tangent_buffer.into_input_buffer_binding(
				u8"TANGENT", 0, static_cast<std::uint32_t>(inputs.size())
			));
			defines.emplace_back(u8"VERTEX_INPUT_HAS_TANGENT", u8"");
		}
		auto shader = _man.compile_shader_in_filesystem(
			_man.get_shader_library_path() / "standard_vertex_shader.hlsl",
			graphics::shader_stage::vertex_shader,
			u8"main_vs",
			defines
		);
		return { std::move(shader), std::move(inputs) };
	}

	assets::handle<assets::shader> pass_context::get_pixel_shader(
		context &ctx, const assets::material::context_data &mat_ctx
	) {
		return _man.compile_shader_in_filesystem(
			_man.get_shader_library_path() / "gbuffer_pixel_shader.hlsl",
			graphics::shader_stage::pixel_shader,
			u8"main_ps",
			{ { u8"LOTUS_MATERIAL_INCLUDE", mat_ctx.get_material_include() } }
		);
	}


	void render_instances(
		context::pass &pass, assets::manager &man, std::span<const instance> instances, mat44f view, mat44f projection
	) {
		pass_context pass_ctx(man);
		for (const auto &inst : instances) {
			graphics_pipeline_state state(
				{
					graphics::render_target_blend_options::disabled(),
					graphics::render_target_blend_options::disabled(),
					graphics::render_target_blend_options::disabled(),
				},
				graphics::rasterizer_options(graphics::depth_bias_options::disabled(), graphics::front_facing_mode::clockwise, graphics::cull_mode::cull_front, false),
				graphics::depth_stencil_options(true, true, graphics::comparison_function::greater, false, 0, 0, graphics::stencil_options::always_pass_no_op(), graphics::stencil_options::always_pass_no_op())
			);
			shader_types::instance_data instance;
			instance.transform = inst.transform;
			instance.view_projection = projection * view;
			resource_set_binding::descriptor_bindings bindings({
				resource_binding(descriptor_resource::immediate_constant_buffer::create_for(instance), 1),
			});
			all_resource_bindings additional_resources = nullptr;
			additional_resources.sets.emplace_back(bindings, 1);
			pass.draw_instanced(
				inst.geometry, inst.material, pass_ctx,
				{}, additional_resources, state, 1, u8"GBuffer instance"
			);
		}
	}
}
