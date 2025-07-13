#pragma once

/// \file
/// Header-only support for Dear ImGui. This is not included by other parts of the library. Include all necessary
/// Dear ImGui headers before this file.

#include <vector>

#include "context/asset_manager.h"
#include "context/constant_uploader.h"
#include "shader_types.h"

namespace lotus::renderer::dear_imgui {
	/// Renderer support for Dear ImGui.
	class context {
	public:
		/// A vertex.
		struct vertex {
			/// No initialization.
			vertex(uninitialized_t) {
			}
			/// Initializes all fields of this struct.
			vertex(cvec2f p, cvec2f u, cvec4f c) : position(p), uv(u), color(c) {
			}

			cvec2f position = uninitialized; ///< Vertex position.
			cvec2f uv       = uninitialized; ///< Vertex UV.
			cvec4f color    = uninitialized; ///< Vertex color.
		};
		using index = u32; ///< Index type.

		/// Creates a new context.
		[[nodiscard]] inline static context create(assets::manager &man, renderer::context::queue q) {
			context result(man, q);

			result._vertex_shader = result._asset_man.compile_shader_in_filesystem(
				result._asset_man.asset_library_path / "shaders/misc/dear_imgui.hlsl",
				gpu::shader_stage::vertex_shader, u8"main_vs"
			);
			result._pixel_shader = result._asset_man.compile_shader_in_filesystem(
				result._asset_man.asset_library_path / "shaders/misc/dear_imgui.hlsl",
				gpu::shader_stage::pixel_shader, u8"main_ps"
			);

			auto &io = ImGui::GetIO();
			io.BackendRendererName = "imgui_impl_lotus_renderer";
			io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

			result._font_texture = _upload_font_texture(io, result._asset_man.get_context(), result._q);
			io.Fonts->SetTexID(result.register_texture(result._font_texture));

			return result;
		}

		/// Renders the current ImGui draw data.
		void render(image2d_color target, cvec2u32 target_size, constant_uploader &uploader, pool buffers_pool) {
			auto *draw_data = ImGui::GetDrawData();
			if (!draw_data) {
				return;
			}

			auto &ctx = _asset_man.get_context();

			cvec2f pos(draw_data->DisplayPos.x, draw_data->DisplayPos.y);
			cvec2f size(draw_data->DisplaySize.x, draw_data->DisplaySize.y);
			cvec2f offset = -2.0f * vec::memberwise_divide(pos, size) - cvec2f(1.0f, 1.0f);
			mat44f projection({
				{ 2.0f / size[0], 0.0f,            0.0f, offset[0]  },
				{ 0.0f,           -2.0f / size[1], 0.0f, -offset[1] },
				{ 0.0f,           0.0f,            0.5f, 0.5f       },
				{ 0.0f,           0.0f,            0.0f, 1.0f       },
			});

			graphics_pipeline_state pipeline(
				{ gpu::render_target_blend_options::create_default_alpha_blend() },
				gpu::rasterizer_options(
					gpu::depth_bias_options::disabled(),
					gpu::front_facing_mode::clockwise,
					gpu::cull_mode::none,
					false
				),
				gpu::depth_stencil_options::all_disabled()
			);

			for (int i = 0; i < draw_data->CmdListsCount; ++i) {
				auto *cmd_list = draw_data->CmdLists[i];

				std::vector<vertex> vertices;
				for (ImDrawVert vert : cmd_list->VtxBuffer) {
					auto color = static_cast<ImVec4>(ImColor(vert.col));
					vertices.emplace_back(
						cvec2f(vert.pos.x, vert.pos.y),
						cvec2f(vert.uv.x, vert.uv.y),
						cvec4f(color.x, color.y, color.z, color.w)
					);
				}
				std::vector<index> indices;
				for (ImDrawIdx idx : cmd_list->IdxBuffer) {
					indices.emplace_back(idx);
				}

				auto vtx_buffer = ctx.request_buffer(
					u8"Dear ImGui Vertex Buffer", sizeof(vertex) * vertices.size(),
					gpu::buffer_usage_mask::copy_destination | gpu::buffer_usage_mask::vertex_buffer,
					buffers_pool
				);
				auto idx_buffer = ctx.request_buffer(
					u8"Dear ImGui Index Buffer", sizeof(index) * indices.size(),
					gpu::buffer_usage_mask::copy_destination | gpu::buffer_usage_mask::index_buffer,
					buffers_pool
				);
				_asset_man.upload_typed_buffer<vertex>(_q, vtx_buffer, vertices);
				_asset_man.upload_typed_buffer<index>(_q, idx_buffer, indices);

				auto pass = _q.begin_pass({ target }, nullptr, target_size, u8"ImGui Draw Pass");
				for (const ImDrawCmd &cmd : cmd_list->CmdBuffer) {
					auto texture_index = cmd.GetTexID();
					shader_types::dear_imgui_draw_data data;
					data.projection = projection;
					data.scissor_min = cvec2f(cmd.ClipRect.x, cmd.ClipRect.y) - pos;
					data.scissor_max = cvec2f(cmd.ClipRect.z, cmd.ClipRect.w) - pos;
					data.uses_texture = texture_index > 0;
					all_resource_bindings resources(
						{
							{ 0, {
								{ 0, uploader.upload(data) },
								{ 1, descriptor_resource::image2d(
									texture_index > 0 ? _registered_images[texture_index - 1] : nullptr,
									image_binding_type::read_only
								) },
							} },
							{ 1, _asset_man.get_samplers() },
						},
						{}
					);

					pass.draw_instanced(
						{
							input_buffer_binding::create(
								vtx_buffer, cmd.VtxOffset * sizeof(vertex),
								gpu::input_buffer_layout::create_vertex_buffer<vertex>({
									{ u8"POSITION", 0, gpu::format::r32g32_float,       offsetof(vertex, position) },
									{ u8"TEXCOORD", 0, gpu::format::r32g32_float,       offsetof(vertex, uv)       },
									{ u8"COLOR",    0, gpu::format::r32g32b32a32_float, offsetof(vertex, color)    },
								}, 0)
							)
						},
						static_cast<u32>(vertices.size()),
						index_buffer_binding(
							idx_buffer, cmd.IdxOffset * sizeof(index), gpu::index_format::uint32
						),
						cmd.ElemCount,
						gpu::primitive_topology::triangle_list,
						std::move(resources),
						_vertex_shader,
						_pixel_shader,
						pipeline,
						1,
						u8"Dear ImGui Draw Call"
					);
				}
				pass.end();
			}
		}

		// TODO this won't work!
		/// Registers a texture to be used with Dear ImGui. This needs to be called every frame the texture is used.
		[[nodiscard]] ImTextureID register_texture(image2d_view img) {
			if (!img) {
				return 0;
			}
			_registered_images.emplace_back(std::move(img));
			return static_cast<ImTextureID>(_registered_images.size());
		}
	private:
		/// Initializes the asset manager.
		explicit context(assets::manager &man, renderer::context::queue q) :
			_asset_man(man), _q(q), _vertex_shader(nullptr), _pixel_shader(nullptr), _font_texture(nullptr) {
		}

		std::vector<recorded_resources::image2d_view> _registered_images;

		assets::manager &_asset_man; ///< The asset manager.
		renderer::context::queue _q; ///< The command queue to render on.
		assets::handle<assets::shader> _vertex_shader; ///< Vertex shader.
		assets::handle<assets::shader> _pixel_shader; ///< Pixel shader.
		image2d_view _font_texture; ///< The font texture.

		/// Uploads the font texture to the GPU, without setting the font texture label.
		[[nodiscard]] static image2d_view _upload_font_texture(
			const ImGuiIO &io,
			renderer::context &rctx,
			renderer::context::queue &q
		) {
			unsigned char *tex_data;
			int width;
			int height;
			io.Fonts->GetTexDataAsRGBA32(&tex_data, &width, &height);

			image2d_view result = rctx.request_image2d(
				u8"Dear ImGui Font Atlas", cvec2i(width, height).into<u32>(), 1,
				gpu::format::r8g8b8a8_unorm,
				gpu::image_usage_mask::copy_destination | gpu::image_usage_mask::shader_read, nullptr
			);
			auto staging_buffer = rctx.request_staging_buffer_for(
				u8"Dear ImGui Font Atlas Staging Buffer", result
			);
			auto *tex_data_bytes = reinterpret_cast<const std::byte*>(tex_data);
			rctx.write_image_data_to_buffer_tight(
				staging_buffer.data, staging_buffer.meta, { tex_data_bytes, tex_data_bytes + width * height * 4 }
			);
			q.copy_buffer_to_image(
				staging_buffer, result, 0, zero, u8"Upload Dear ImGui Font Atlas"
			);
			return result;
		}
	};
}
