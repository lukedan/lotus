#include "lotus/renderer/g_buffer.h"

/// \file
/// Implementation of the G-buffer.

namespace lotus::renderer::g_buffer {
	static const std::array _color_render_target_formats = {
		graphics::format::r8g8b8a8_unorm,
		graphics::format::r16g16b16a16_float,
		graphics::format::r8_unorm,
	};
	static const graphics::format _depth_stencil_render_target_format = graphics::format::d32_float_s8;


	const graphics::frame_buffer_layout &get_frame_buffer_layout() {
		static graphics::frame_buffer_layout _layout(
			_color_render_target_formats, _depth_stencil_render_target_format
		);

		return _layout;
	}


	view view::create(context &ctx, cvec2s size) {
		view result = nullptr;
		result.albedo_glossiness = ctx.request_surface2d(u8"Albedo-glossiness", size, _color_render_target_formats[0]);
		result.normal            = ctx.request_surface2d(u8"Normal",            size, _color_render_target_formats[1]);
		result.metalness         = ctx.request_surface2d(u8"Metalness",         size, _color_render_target_formats[2]);
		result.depth_stencil     = ctx.request_surface2d(u8"Depth-stencil",     size, graphics::format::d32_float_s8);
		return result;
	}


	void render_instances(context &ctx, std::span<const instance> instances) {
		/*auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto vert_buffers = bookmark.create_vector_array<graphics::vertex_buffer>();
		auto &cmd_list = ctx.get_command_list();
		for (const auto &inst : instances) {
			// collect vertex buffers
			const auto &geom = inst.geometry.get().value;
			vert_buffers.clear();
			vert_buffers.emplace_back(graphics::vertex_buffer::from_buffer_offset_stride(
				geom.vertex_buffer, 0, sizeof(cvec3f)
			));
			if (geom.uv_buffer) {
				vert_buffers.emplace_back(graphics::vertex_buffer::from_buffer_offset_stride(
					geom.uv_buffer, 0, sizeof(cvec2f)
				));
			}
			if (geom.normal_buffer) {
				vert_buffers.emplace_back(graphics::vertex_buffer::from_buffer_offset_stride(
					geom.normal_buffer, 0, sizeof(cvec3f)
				));
			}
			if (geom.tangent_buffer) {
				vert_buffers.emplace_back(graphics::vertex_buffer::from_buffer_offset_stride(
					geom.tangent_buffer, 0, sizeof(cvec4f)
				));
			}

			cmd_list.bind_pipeline_state(inst.pipeline.pipeline_state);
			cmd_list.bind_vertex_buffers(0, vert_buffers);
			if (geom.index_buffer) {
				cmd_list.bind_index_buffer(geom.index_buffer, 0, graphics::index_format::uint32);
				cmd_list.draw_indexed_instanced(0, geom.num_indices, 0, 0, 1);
			} else {
				cmd_list.draw_instanced(0, geom.num_vertices, 0, 1);
			}
		}*/
	}
}
