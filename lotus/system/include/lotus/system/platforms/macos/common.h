#pragma once

/// \file
/// Used to include platform-specific headers for MacOS.

#include "lotus/system/common.h"

namespace lotus::system::platforms::macos {
}

namespace lotus::system::platform {
	using namespace platforms::macos;

	constexpr static platform_type type = platform_type::macos; ///< Current platform type.
}
