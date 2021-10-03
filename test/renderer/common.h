#pragma once

#include <tiny_gltf.h>

namespace gltf = tinygltf;

using namespace lotus;
namespace sys = lotus::system;
namespace gfx = lotus::graphics;

inline std::vector<std::byte> load_binary_file(const std::filesystem::path &p) {
	std::ifstream fin(p, std::ios::binary | std::ios::ate);
	assert(fin.good());
	std::vector<std::byte> result(fin.tellg());
	fin.seekg(0, std::ios::beg);
	fin.read(reinterpret_cast<char*>(result.data()), result.size());
	return result;
}
