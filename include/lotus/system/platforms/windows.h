#pragma once

/// \file
/// Used to include platform-specific headers for Windows.

#include "windows/application.h"
#include "windows/window.h"

namespace lotus::system {
	namespace platform = platforms::windows;
}
