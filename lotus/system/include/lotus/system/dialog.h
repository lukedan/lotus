#pragma once

/// \file
/// Common system dialogs.

#include <string_view>
#include <filesystem>

#include "common.h"
#include LOTUS_SYSTEM_PLATFORM_INCLUDE_DIALOG

namespace lotus::system {
	class window;
}

namespace lotus::system::dialog {
	namespace message_box {
		/// Shows a message box with the given parameters.
		void show(std::u8string_view contents, std::u8string_view caption, style s, window *parent = nullptr) {
			platform::dialog::message_box::show(contents, caption, s, parent);
		}
	}

	namespace file {
		/// Shows a dialog that allows the user to choose a single existing file.
		std::filesystem::path open(window *parent = nullptr) {
			return platform::dialog::file::open(parent);
		}
	}
}
