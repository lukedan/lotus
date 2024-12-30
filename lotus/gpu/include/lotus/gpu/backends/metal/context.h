#pragma once

/// \file
/// Metal contexts.

#include <filesystem>
#include <vector>
#include <span>

#include "lotus/system/window.h"
#include "lotus/gpu/common.h"
#include "details.h"
#include "device.h"
#include "frame_buffer.h"

namespace lotus::gpu::backends::metal {
	/// The Metal API is global; the class contains no data members.
	class context {
	protected:
		using debug_message_id = _details::debug_message_id; ///< Debug message ID type.

		/// Returns a new context object.
		[[nodiscard]] static context create(context_options, _details::debug_message_callback);

		/// Calls \p MTL::CopyAllDevices().
		[[nodiscard]] std::vector<adapter> get_all_adapters() const;

		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::window&,
			device&,
			command_queue&,
			std::size_t frame_count,
			std::span<const format> formats
		); // TODO
	};

	// TODO
	class shader_utility {
	protected:
		// TODO
		struct compilation_result {
		protected:
			[[nodiscard]] bool succeeded() const; // TODO
			[[nodiscard]] std::u8string_view get_compiler_output(); // TODO
			[[nodiscard]] std::span<const std::byte> get_compiled_binary(); // TODO
		};

		[[nodiscard]] static shader_utility create(); // TODO

		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte> data); // TODO
		[[nodiscard]] shader_library_reflection load_shader_library_reflection(std::span<const std::byte> data); // TODO
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8, shader_stage, std::u8string_view entry,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		); // TODO
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code_utf8,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		); // TODO
	};
}
