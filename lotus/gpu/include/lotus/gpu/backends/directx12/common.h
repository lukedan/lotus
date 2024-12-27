#pragma once

/// \file
/// Sets up DirectX 12 to be the graphics backend.

#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::directx12 {
}

namespace lotus::gpu::backend {
	using namespace backends::directx12;

	constexpr static backend_type type = backend_type::directx12; ///< Backend type.
}
