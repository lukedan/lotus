#pragma once

/// \file
/// Common utilities.

#include <string>
#include <format>

#include "lotus/utils/strings.h"

namespace lotus {
	namespace system {
	}
	namespace gpu {
	}
	namespace renderer {
	}
	namespace string {
	}
}

namespace lsys = lotus::system;
namespace lgpu = lotus::gpu;
namespace lren = lotus::renderer;
namespace lstr = lotus::string;

using lotus::uninitialized_t;
using lotus::uninitialized;
using lotus::zero_t;
using lotus::zero;

/// \p std::format with UTF-8 encoding.
template <
	lotus::string::constexpr_string fmt_str, typename ...Args
> [[nodiscard]] inline std::u8string format_utf8(Args &&...args) {
	constexpr auto fmt_str_char = fmt_str.as<char>();
	std::string result = std::format(std::locale("en_US.UTF-8"), fmt_str_char, std::forward<Args>(args)...);
	return std::u8string(reinterpret_cast<const char8_t*>(result.data()), result.size());
}
