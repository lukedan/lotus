#pragma once

/// \file
/// Conversion to and from \p DXGI_FORMAT.

#include <directx/d3d12.h>

#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::common::_details::conversions {
	/// Converts a \ref format to a \p DXGI_FORMAT.
	[[nodiscard]] DXGI_FORMAT to_format(format);

	/// Converts a \p DXGI_FORMAT back to a \ref format.
	[[nodiscard]] format back_to_format(DXGI_FORMAT);
}
