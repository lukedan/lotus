#pragma once

/// \file
/// Definitions of MoltenVK related functions.

#include "lotus/system/window.h"
#include "details.h"

namespace lotus::gpu::backends::vulkan::_details {
	/// Creates a \p CAMetalLayer for the given window.
	[[nodiscard]] CAMetalLayer *create_core_animation_metal_layer(system::window::native_handle_t);
}
