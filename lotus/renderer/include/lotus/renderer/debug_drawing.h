#pragma once

/// \file
/// Utility class for debug drawing.

#include <span>

#include "context/context.h"
#include "context/asset_manager.h"

namespace lotus::renderer {
	/// A basic debug renderer.
	class debug_renderer {
	public:
		/// A vertex.
		struct vertex {
			/// Does not initialize this struct.
			vertex(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			vertex(cvec3f p, cvec4f c) : position(p), color(c) {
			}

			cvec3f position = uninitialized; ///< Position of the vertex.
			cvec4f color = uninitialized; ///< Color of the vertex.
		};

		/// Creates a valid debug renderer object.
		[[nodiscard]] inline static debug_renderer create(assets::manager &man) {
			debug_renderer result(man.get_context());
			result._vertex_shader = man.compile_shader_in_filesystem(
				man.shader_library_path / "debug_untextured.hlsl", gpu::shader_stage::vertex_shader, u8"main_vs"
			);
			result._pixel_shader = man.compile_shader_in_filesystem(
				man.shader_library_path / "debug_untextured.hlsl", gpu::shader_stage::pixel_shader, u8"main_ps"
			);
			return result;
		}

		/// Adds the given vertices as lines.
		void add_line_vertices(std::span<const vertex> verts) {
			crash_if(verts.size() % 2 != 0);
			_lines.insert(_lines.end(), verts.begin(), verts.end());
		}
		/// Adds the given vertices as triangles.
		void add_triangle_vertices(std::span<const vertex> verts) {
			crash_if(verts.size() % 3 != 0);
			_triangles.insert(_triangles.end(), verts.begin(), verts.end());
		}

		/// Renders all accumulated contents to the given target and resets the vertex buffers.
		void flush(
			image2d_color target, image2d_depth_stencil depth_stencil, cvec2s size,
			const shader_types::view_data &view, std::u8string_view description = u8"Debug Draw"
		) {
			_do_flush(_lines, gpu::primitive_topology::line_list, target, depth_stencil, size, view, description);
			_do_flush(
				_triangles, gpu::primitive_topology::triangle_list, target, depth_stencil, size, view, description
			);
		}


		/// Adds a line segment.
		void add_line(cvec3f p1, cvec3f p2, linear_rgba_f color) {
			vertex vs[] = { vertex(p1, color.into_vector()), vertex(p2, color.into_vector()) };
			add_line_vertices(vs);
		}

		/// Adds a simple locator composed of three axis-aligned line segments.
		void add_locator(cvec3f pos, linear_rgba_f color, float size = 0.1f) {
			add_line(pos - cvec3f(size, 0.0f, 0.0f), pos + cvec3f(size, 0.0f, 0.0f), color);
			add_line(pos - cvec3f(0.0f, size, 0.0f), pos + cvec3f(0.0f, size, 0.0f), color);
			add_line(pos - cvec3f(0.0f, 0.0f, size), pos + cvec3f(0.0f, 0.0f, size), color);
		}
	private:
		/// Only initializes \ref _context.
		explicit debug_renderer(context &ctx) : _context(ctx), _vertex_shader(nullptr), _pixel_shader(nullptr) {
		}

		std::vector<vertex> _lines; ///< A list of lines.
		std::vector<vertex> _triangles; ///< A list of triangles.

		context &_context; ///< The rendering context.
		assets::handle<assets::shader> _vertex_shader; ///< Vertex shader.
		assets::handle<assets::shader> _pixel_shader; ///< Pixel shader.

		/// Flushes the given vertex buffer with the given topology.
		void _do_flush(
			std::vector<vertex> &vertices, gpu::primitive_topology topology,
			image2d_color target, image2d_depth_stencil depth_stencil, cvec2s size,
			const shader_types::view_data &view,
			std::u8string_view description = u8"Debug Draw"
		) {
			if (vertices.empty()) {
				return;
			}

			auto vert_buf = _context.request_buffer(
				u8"Debug Draw Vertex Buffer", vertices.size() * sizeof(vertex),
				gpu::buffer_usage_mask::copy_destination | gpu::buffer_usage_mask::vertex_buffer, nullptr
			);
			_context.upload_buffer<vertex>(vert_buf, vertices, 0, u8"Upload Debug Vertex Buffer");

			auto resource_bindings = all_resource_bindings::from_unsorted({
				resource_set_binding::descriptors({
					descriptor_resource::immediate_constant_buffer::create_for(view).at_register(0),
				}).at_space(0),
			});
			graphics_pipeline_state pipeline_state(
				{ gpu::render_target_blend_options::create_default_alpha_blend(), },
				nullptr,
				gpu::depth_stencil_options(
					true, true, gpu::comparison_function::greater,
					false, 0, 0, gpu::stencil_options::always_pass_no_op(), gpu::stencil_options::always_pass_no_op()
				)
			);

			auto pass = _context.begin_pass({ target }, depth_stencil, size, description);
			pass.draw_instanced(
				{
					input_buffer_binding(0, vert_buf, 0, sizeof(vertex), gpu::input_buffer_rate::per_vertex, {
						gpu::input_buffer_element(u8"POSITION", 0, gpu::format::r32g32b32_float,    offsetof(vertex, position)),
						gpu::input_buffer_element(u8"TEXCOORD", 0, gpu::format::r32g32b32a32_float, offsetof(vertex, color)),
					}),
				},
				vertices.size(), nullptr, 0, topology,
				std::move(resource_bindings), _vertex_shader, _pixel_shader, std::move(pipeline_state), 1,
				description
			);
			pass.end();

			vertices.clear();
		}
	};
}
