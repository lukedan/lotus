#pragma once

/// \file
/// Miscellaneous functions used by the MacOS platform.

#include "lotus/system/common.h"

@class NSString;

namespace lotus::system::platforms::macos::_details {
	/// Conversions.
	namespace conversions {
		/// Converts a Carbon key code to a \ref key enum.
		[[nodiscard]] key to_key(u16 code);

		/// Converts \p std::u8string_view to a \p NSString. The caller needs to manually release or autorelease this
		/// string.
		[[nodiscard]] NSString *to_ns_string(std::u8string_view);
	}
}
