#pragma once

/// \file
/// Renders geometry onto a G-buffer.

#include "context/context.h"

namespace lotus::renderer::g_buffer {
	/// Storage for the G-buffer.
	struct view {
	public:
		/// Format of \ref albedo_glossiness.
		constexpr static gpu::format albedo_glossiness_format = gpu::format::r8g8b8a8_unorm;
		/// Format of \ref normal.
		constexpr static gpu::format normal_format            = gpu::format::r16g16b16a16_snorm;
		/// Format of \ref metalness.
		constexpr static gpu::format metalness_format         = gpu::format::r8_unorm;
		/// Format of \ref velocity.
		constexpr static gpu::format velocity_format          = gpu::format::r16g16_float;
		/// Format of \ref depth_stencil.
		constexpr static gpu::format depth_stencil_format     = gpu::format::d24_unorm_s8;

		/// Initializes this storage to empty.
		view(std::nullptr_t) :
			albedo_glossiness(nullptr),
			normal(nullptr),
			metalness(nullptr),
			velocity(nullptr),
			depth_stencil(nullptr) {
		}
		/// Creates a storage with the given size.
		[[nodiscard]] static view create(context&, cvec2s size, const pool&);

		/// Starts a pass rendering to this view.
		[[nodiscard]] context::pass begin_pass(context&);

		image2d_view albedo_glossiness; ///< Albedo and glossiness buffer.
		image2d_view normal;            ///< Normal buffer.
		image2d_view metalness;         ///< Metalness buffer.
		image2d_view velocity;          ///< Velocity buffer.
		image2d_view depth_stencil;     ///< Depth-stencil buffer.
	};

	/// Pass context for the GBuffer pass.
	class pass_context : public renderer::pass_context {
	public:
		/// Initializes the asset manager.
		explicit pass_context(assets::manager &man) : _man(man) {
		}

		/// Retrieves a vertex shader and vertex inputs.
		[[nodiscard]] std::pair<assets::handle<assets::shader>, std::vector<input_buffer_binding>> get_vertex_shader(
			context&, const assets::material::context_data&, const assets::geometry&
		) override;
		/// Retrieves a pixel shader.
		[[nodiscard]] assets::handle<assets::shader> get_pixel_shader(
			context&, const assets::material::context_data&
		) override;
	private:
		assets::manager &_man; ///< The associated asset manager.
	};

	/// Renders the given instances in the given pass.
	void render_instances(
		context::pass&, assets::manager&, std::span<const instance>,
		cvec2u32 viewport_size, mat44f view, mat44f projection, mat44f jitter, mat44f prev_view_projection
	);
}
