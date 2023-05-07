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
		/// A vertex without UV coordinates.
		struct vertex_untextured {
			/// Does not initialize this struct.
			vertex_untextured(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			vertex_untextured(cvec3f p, cvec4f c) : position(p), color(c) {
			}

			cvec3f position = uninitialized; ///< Position of the vertex.
			cvec4f color = uninitialized; ///< Color of the vertex.
		};
		/// A vertex with UV coordinates.
		struct vertex_textured {
			/// Does not initialize this struct.
			vertex_textured(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			vertex_textured(cvec3f p, cvec4f c, cvec2f u) : position(p), color(c), uv(u) {
			}

			cvec3f position = uninitialized; ///< Position of the vertex.
			cvec4f color = uninitialized; ///< Color of the vertex.
			cvec2f uv = uninitialized; ///< UV of the vertex.
		};

		/// Creates a valid debug renderer object.
		[[nodiscard]] inline static debug_renderer create(assets::manager &man, context::queue q) {
			debug_renderer result(man, q);

			result._vertex_shader_untextured = man.compile_shader_in_filesystem(
				man.asset_library_path / "shaders/misc/debug_untextured.hlsl", gpu::shader_stage::vertex_shader, u8"main_vs"
			);
			result._pixel_shader_untextured = man.compile_shader_in_filesystem(
				man.asset_library_path / "shaders/misc/debug_untextured.hlsl", gpu::shader_stage::pixel_shader, u8"main_ps"
			);

			result._vertex_shader_textured = man.compile_shader_in_filesystem(
				man.asset_library_path / "shaders/misc/debug_textured.hlsl", gpu::shader_stage::vertex_shader, u8"main_vs"
			);
			result._pixel_shader_textured = man.compile_shader_in_filesystem(
				man.asset_library_path / "shaders/misc/debug_textured.hlsl", gpu::shader_stage::pixel_shader, u8"main_ps"
			);

			return result;
		}

		/// Adds the given vertices as lines.
		void add_line_vertices_untextured(std::span<const vertex_untextured> verts) {
			crash_if(verts.size() % 2 != 0);
			_lines_untextured.insert(_lines_untextured.end(), verts.begin(), verts.end());
		}
		/// Adds the given vertices as triangles.
		void add_triangle_vertices_untextured(std::span<const vertex_untextured> verts) {
			crash_if(verts.size() % 3 != 0);
			_triangles_untextured.insert(_triangles_untextured.end(), verts.begin(), verts.end());
		}
		/// Adds the given vertices as triangles.
		void add_triangle_vertices_textured(
			std::span<const vertex_textured> verts, image2d_view tex
		) {
			crash_if(verts.size() % 3 != 0);
			if (!_triangles_textured.empty() && _triangles_textured.back().texture == tex) {
				_triangles_textured.back().vertices.insert(
					_triangles_textured.back().vertices.end(), verts.begin(), verts.end()
				);
			} else {
				auto &batch = _triangles_textured.emplace_back(nullptr);
				batch.texture = std::move(tex);
				batch.vertices = std::vector(verts.begin(), verts.end());
			}
		}

		/// Renders all accumulated contents to the given target and resets the vertex buffers.
		void flush(
			image2d_color target, image2d_depth_stencil depth_stencil, cvec2u32 size,
			mat44f projection, std::u8string_view description = u8"Debug Draw"
		) {
			_do_flush(
				_lines_untextured, gpu::primitive_topology::line_list,
				target, depth_stencil, size, projection, nullptr, description
			);
			_do_flush(
				_triangles_untextured, gpu::primitive_topology::triangle_list,
				target, depth_stencil, size, projection, nullptr, description
			);

			for (auto &batch : _triangles_textured) {
				_do_flush(
					batch.vertices, gpu::primitive_topology::triangle_list,
					target, depth_stencil, size, projection, std::move(batch.texture), description
				);
			}
			_triangles_textured.clear();
		}


		/// Adds a line segment.
		void add_line(cvec3f p1, cvec3f p2, linear_rgba_f color) {
			vertex_untextured vs[] = {
				vertex_untextured(p1, color.into_vector()),
				vertex_untextured(p2, color.into_vector()),
			};
			add_line_vertices_untextured(vs);
		}

		/// Adds a simple locator composed of three axis-aligned line segments.
		void add_locator(cvec3f pos, linear_rgba_f color, float size = 0.1f) {
			add_line(pos - cvec3f(size, 0.0f, 0.0f), pos + cvec3f(size, 0.0f, 0.0f), color);
			add_line(pos - cvec3f(0.0f, size, 0.0f), pos + cvec3f(0.0f, size, 0.0f), color);
			add_line(pos - cvec3f(0.0f, 0.0f, size), pos + cvec3f(0.0f, 0.0f, size), color);
		}
	private:
		/// Only initializes \ref _asset_man.
		explicit debug_renderer(assets::manager &man, context::queue q) :
			_asset_man(man), _q(q),
			_vertex_shader_untextured(nullptr), _pixel_shader_untextured(nullptr),
			_vertex_shader_textured(nullptr), _pixel_shader_textured(nullptr) {
		}

		/// A batch of vertices that use the same texture.
		struct _textured_batch {
			/// Initializes this batch to empty.
			_textured_batch(std::nullptr_t) : texture(nullptr) {
			}

			image2d_view texture; ///< The texture.
			std::vector<vertex_textured> vertices; ///< Vertices.
		};

		std::vector<vertex_untextured> _lines_untextured; ///< A list of lines.
		std::vector<vertex_untextured> _triangles_untextured; ///< A list of untextured triangles.

		std::vector<_textured_batch> _triangles_textured; ///< Batches of textured triangles.

		assets::manager &_asset_man; ///< The asset manager.
		context::queue _q; ///< The command queue to render on.
		assets::handle<assets::shader> _vertex_shader_untextured; ///< Untextured vertex shader.
		assets::handle<assets::shader> _pixel_shader_untextured; ///< Untextured pixel shader.
		assets::handle<assets::shader> _vertex_shader_textured; ///< Textured vertex shader.
		assets::handle<assets::shader> _pixel_shader_textured; ///< Textured pixel shader.

		/// Flushes the given vertex buffer with the given topology.
		template <typename Vert> void _do_flush(
			std::vector<Vert> &vertices, gpu::primitive_topology topology,
			image2d_color target, image2d_depth_stencil depth_stencil, cvec2u32 size,
			mat44f projection, image2d_view texture,
			std::u8string_view description = u8"Debug Draw"
		) {
			if (vertices.empty()) {
				return;
			}

			auto &ctx = _asset_man.get_context();

			auto vert_buf = ctx.request_buffer(
				u8"Debug Draw Vertex Buffer", vertices.size() * sizeof(Vert),
				gpu::buffer_usage_mask::copy_destination | gpu::buffer_usage_mask::vertex_buffer, nullptr
			);
			_q.upload_buffer<Vert>(vert_buf, vertices, 0, u8"Upload Debug Vertex Buffer");

			all_resource_bindings resource_bindings = nullptr;
			input_buffer_binding input_bindings = nullptr;
			assets::handle<assets::shader> vs = nullptr;
			assets::handle<assets::shader> ps = nullptr;

			shader_types::debug_draw_data data;
			data.projection = projection;
			if constexpr (std::is_same_v<Vert, vertex_untextured>) {
				resource_bindings = all_resource_bindings(
					{
						{ 0, {
							{ 0, descriptor_resource::immediate_constant_buffer::create_for(data) },
						} },
					},
					{}
				);
				input_bindings = input_buffer_binding(
					0, vert_buf, 0, sizeof(Vert), gpu::input_buffer_rate::per_vertex, {
						{ u8"POSITION", 0, gpu::format::r32g32b32_float,    offsetof(Vert, position) },
						{ u8"COLOR",    0, gpu::format::r32g32b32a32_float, offsetof(Vert, color)    },
					}
				);
				vs = _vertex_shader_untextured;
				ps = _pixel_shader_untextured;
			} else if constexpr (std::is_same_v<Vert, vertex_textured>) {
				resource_bindings = all_resource_bindings(
					{
						{ 0, {
							{ 0, descriptor_resource::immediate_constant_buffer::create_for(data) },
							{ 1, texture.bind_as_read_only() },
						} },
						{ 1, _asset_man.get_samplers() },
					},
					{}
				);
				input_bindings = input_buffer_binding::create(
					vert_buf, 0, gpu::input_buffer_layout::create_vertex_buffer<Vert>({
						{ u8"POSITION", 0, gpu::format::r32g32b32_float,    offsetof(Vert, position) },
						{ u8"COLOR",    0, gpu::format::r32g32b32a32_float, offsetof(Vert, color)    },
						{ u8"TEXCOORD", 0, gpu::format::r32g32_float,       offsetof(Vert, uv)       },
					}, 0)
				);
				vs = _vertex_shader_textured;
				ps = _pixel_shader_textured;
			} else {
				static_assert(!std::is_same_v<Vert, Vert>, "Invalid vertex type");
			}

			graphics_pipeline_state pipeline_state(
				{ gpu::render_target_blend_options::create_default_alpha_blend(), },
				nullptr,
				gpu::depth_stencil_options(
					true, true, gpu::comparison_function::greater,
					false, 0, 0, gpu::stencil_options::always_pass_no_op(), gpu::stencil_options::always_pass_no_op()
				)
			);

			auto pass = _q.begin_pass({ target }, depth_stencil, size, description);
			pass.draw_instanced(
				{ std::move(input_bindings) },
				vertices.size(), nullptr, 0, topology,
				std::move(resource_bindings), std::move(vs), std::move(ps), std::move(pipeline_state), 1,
				description
			);
			pass.end();

			vertices.clear();
		}
	};
}
