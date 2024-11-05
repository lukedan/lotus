#pragma once

/// \file
/// Miscellaneous utilities.

#include <cstdio>

#include "lotus/memory/common.h"
#include "lotus/memory/block.h"

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


	/// Converts the given four-character literal to its 32-bit binary representation.
	constexpr inline static std::uint32_t make_four_character_code(std::u8string_view s) {
		return
			static_cast<std::uint32_t>(s[0]) |
			(static_cast<std::uint32_t>(s[1]) << 8) |
			(static_cast<std::uint32_t>(s[2]) << 16) |
			(static_cast<std::uint32_t>(s[3]) << 24);
	}

	/// Helper class that enables overloading of lambdas and function objects.
	template <typename... Ts> struct overloaded : Ts... {
	public:
		using Ts::operator()...;
	};
	template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;
}
