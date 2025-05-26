#pragma once

/// \file
/// Miscellaneous functions used by the MacOS platform.

#include "lotus/system/common.h"

namespace lotus::system::platforms::macos::_details {
	/// Conversions.
	namespace conversions {
		/// Converts a Carbon key code to a \ref key enum.
		[[nodiscard]] key to_key(u16 code);
	}
}
