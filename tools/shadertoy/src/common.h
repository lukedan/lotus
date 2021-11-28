#pragma once

/// \file
/// Common utilities.

#include <string>
#include <format>

namespace lotus {
	namespace system {
	}
	namespace graphics {
	}
}

namespace lsys = lotus::system;
namespace lgfx = lotus::graphics;

using lotus::uninitialized_t;
using lotus::uninitialized;
using lotus::zero_t;
using lotus::zero;

/// Converts a UTF-8 string view into a generic string view.
[[nodiscard]] inline std::string_view as_string(std::u8string_view sv) {
	return std::string_view(reinterpret_cast<const char*>(sv.data()), sv.size());
}

/// \p std::format with UTF-8 encoding.
template <typename ...Args> [[nodiscard]] inline std::u8string format_utf8(std::u8string fmt_str, Args &&...args) {
	std::string result = std::format(
		std::locale("en_US.UTF-8"),
		std::string_view(reinterpret_cast<const char*>(fmt_str.data()), fmt_str.size()),
		std::forward<Args>(args)...
	);
	return std::u8string(reinterpret_cast<const char8_t*>(result.data()), result.size());
}

/// Loads a file as binary.
inline std::vector<std::byte> load_binary_file(const std::filesystem::path &p) {
	std::vector<std::byte> result;
	std::ifstream fin(p, std::ios::binary | std::ios::ate);
	if (!fin.good()) {
		return result;
	}
	result.resize(fin.tellg());
	fin.seekg(0, std::ios::beg);
	fin.read(reinterpret_cast<char*>(result.data()), result.size());
	return result;
}
