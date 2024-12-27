#pragma once

/// \file
/// Sets up Vulkan to be the graphics backend.

#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::vulkan {
}

namespace lotus::gpu::backend {
	using namespace backends::vulkan;

	constexpr static backend_type type = backend_type::vulkan; ///< Backend type.
}
