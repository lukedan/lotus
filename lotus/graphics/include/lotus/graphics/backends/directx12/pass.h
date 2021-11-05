#pragma once

/// \file
/// DirectX 12 render passes.

#include <array>

#include "details.h"

namespace lotus::graphics::backends::directx12 {
	class command_list;
	class device;

	/// Contains all parameters passed to \p BeginRenderPass(), except for the descriptors.
	class pass_resources {
		friend command_list;
		friend device;
	protected:
		/// No initialization.
		pass_resources(std::nullptr_t) {
		}
	private:
		/// Render target information of the pass.
		std::array<D3D12_RENDER_PASS_RENDER_TARGET_DESC, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> _render_targets;
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC _depth_stencil; ///< Depth stencil information of the pass.
		std::array<DXGI_FORMAT, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> _render_target_format; ///< Render target format.
		DXGI_FORMAT _depth_stencil_format; ///< Depth stencil format.
		D3D12_RENDER_PASS_FLAGS _flags; ///< Render pass flags.
		std::uint8_t _num_render_targets; ///< The number of render targets in \ref _render_targets.
	};
}
