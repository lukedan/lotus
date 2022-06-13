#pragma once

/// \file
/// Renders geometry onto a G-buffer.

#include "context.h"

namespace lotus::renderer::g_buffer {
	/// Returns the frame buffer layout of the G-buffer.
	[[nodiscard]] const graphics::frame_buffer_layout &get_frame_buffer_layout();

	/// Storage for the G-buffer.
	struct view {
	public:
		/// Initializes this storage to empty.
		view(std::nullptr_t) :
			albedo_glossiness(nullptr), normal(nullptr), metalness(nullptr), depth_stencil(nullptr) {
		}
		/// Creates a storage with the given size.
		[[nodiscard]] static view create(context&, cvec2s size);

		image2d_view albedo_glossiness; ///< Albedo and glossiness buffer.
		image2d_view normal;            ///< Normal buffer.
		image2d_view metalness;         ///< Metalness buffer.
		image2d_view depth_stencil;     ///< Depth-stencil buffer.
	};

	/// Renders the given instances onto the given storage.
	void render_instances(context&, std::span<const instance>);
}
