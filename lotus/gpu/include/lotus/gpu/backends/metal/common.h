#pragma once

/// \file
/// Sets up Metal to be the graphics backend.

#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::metal {
}

namespace lotus::gpu::backend {
	using namespace backends::metal;

	constexpr static backend_type type = backend_type::metal; ///< Backend type.
}
