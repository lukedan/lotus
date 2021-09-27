#pragma once

/// \file
/// DirectX 12 render passes.

#include <array>

#include <d3d12.h>

namespace lotus::graphics::backends::directx12 {
	class command_list;
	class device;

	/// Contains all parameters passed to \p BeginRenderPass(), except for the descriptors.
	class pass_resources {
		friend command_list;
		friend device;
	protected:
	private:
		/// Render target information of the pass.
		std::array<D3D12_RENDER_PASS_RENDER_TARGET_DESC, num_color_render_targets> _render_targets;
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC _depth_stencil; ///< Depth stencil information of the pass.
		std::array<DXGI_FORMAT, num_color_render_targets> _render_target_format; ///< Render target format.
		DXGI_FORMAT _depth_stencil_format; ///< Depth stencil format.
		D3D12_RENDER_PASS_FLAGS _flags; ///< Render pass flags.
		std::uint8_t _num_render_targets; ///< The number of render targets in \ref _render_targets.
	};
}