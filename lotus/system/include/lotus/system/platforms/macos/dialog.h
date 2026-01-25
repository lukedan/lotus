#pragma once

/// \file
/// MacOS dialog boxes.

#include <string_view>
#include <filesystem>

#include "lotus/system/common.h"

namespace lotus::system {
	class window;
}

namespace lotus::system::platforms::macos::dialog {
	using namespace system::dialog;

	namespace message_box {
		using namespace system::dialog::message_box;

		/// Creates a \p NSAlert and shows it.
		void show(std::u8string_view contents, std::u8string_view caption, style, system::window *parent);
	}

	namespace file {
		/// Creates a \p NSOpenPanel and shows it.
		std::filesystem::path open(system::window *parent);
	}
}
