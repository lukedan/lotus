#pragma once

/// \file
/// Conversion to and from \p DXGI_FORMAT.

#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::common::_details::conversions {
	/// Enum class type that corresponds to \p DXGI_FORMAT.
	enum class dxgi_format : u32 {
	};

	/// Converts a \ref format to a \ref dxgi_format.
	[[nodiscard]] dxgi_format to_dxgi_format(format);

	/// Converts a \ref dxgi_format back to a \ref format.
	[[nodiscard]] format back_to_format(dxgi_format);
}
