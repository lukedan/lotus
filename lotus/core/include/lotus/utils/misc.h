#pragma once

/// \file
/// Miscellaneous utilities.

#include <filesystem>

#include "lotus/memory/common.h"
#include "lotus/memory/block.h"

namespace lotus {
	/// Loads the specified file as binary. The size of the file will be passed to the callback function, which
	/// should allocate and return storage for the contents of the file.
	///
	/// \return Whether the file was successfully loaded.
	[[nodiscard]] bool load_binary_file(
		const std::filesystem::path&, std::span<std::byte>(*allocate)(std::size_t, void*), void *user_data
	);
	/// \overload
	template <typename Callback> [[nodiscard]] bool load_binary_file(
		const std::filesystem::path &path, Callback &&allocate
	) {
		return load_binary_file(path,
			[](std::size_t sz, void *user_data) {
				return (*static_cast<std::remove_reference_t<Callback&&>*>(user_data))(sz);
			},
			&allocate
		);
	}
	/// \overload
	template <
		typename Allocator = memory::raw::allocator
	> [[nodiscard]] std::pair<memory::block<Allocator>, std::size_t> load_binary_file(
		const std::filesystem::path &path, const Allocator &alloc = Allocator{}, std::size_t alignment = 1
	) {
		memory::block<Allocator> result(nullptr, alloc);
		std::size_t result_size;
		const bool success = load_binary_file(path,
			[&](std::size_t sz) -> std::span<std::byte> {
				result = memory::allocate_block(memory::size_alignment(sz, alignment), alloc);
				result_size = sz;
				return { result.get(), result_size };
			}
		);
		if (!success) {
			result.reset();
			result_size = 0;
		}
		return { std::move(result), result_size };
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
