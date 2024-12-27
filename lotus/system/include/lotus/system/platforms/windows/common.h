#pragma once

/// \file
/// Used to include platform-specific headers for Windows.

#include "lotus/system/common.h"

namespace lotus::system::platforms::windows {
}

namespace lotus::system::platform {
	using namespace platforms::windows;

	constexpr static platform_type type = platform_type::windows; ///< Current platform type.
}
