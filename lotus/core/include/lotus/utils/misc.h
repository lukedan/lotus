#pragma once

/// \file
/// Miscellaneous utilities.

#include <cstdio>

#include "lotus/memory.h"

namespace lotus {
	/// Loads the specified binary file.
	template <
		typename Allocator = memory::raw::allocator
	> std::pair<memory::block<Allocator>, std::size_t> load_binary_file(
		const char *path, Allocator alloc = Allocator{}, std::size_t alignment = 1
	) {
		FILE *fin = std::fopen(path, "rb");
		if (fin) {
			if (std::fseek(fin, 0, SEEK_END) == 0) {
				long pos = std::ftell(fin);
				if (pos >= 0) {
					std::rewind(fin);
					auto size = static_cast<std::size_t>(pos);
					auto block = memory::block<Allocator>::allocate(
						memory::size_alignment(size, alignment), std::move(alloc)
					);
					std::size_t bytes_read = std::fread(block.get(), 1, size, fin);
					if (bytes_read == size) {
						std::fclose(fin);
						return std::make_pair(std::move(block), size);
					}
				}
			}
			std::fclose(fin);
		}
		return { memory::block<Allocator>(nullptr, std::move(alloc)), 0};
	}
}
