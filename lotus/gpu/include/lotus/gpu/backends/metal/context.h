#pragma once

/// \file
/// Metal contexts.

#include <filesystem>
#include <vector>
#include <span>

#include "lotus/system/window.h"
#include "lotus/gpu/common.h"
#include "lotus/gpu/backends/common/dxc.h"
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

		/// Creates a \p CAMetalLayer and assigns it to the window.
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::window&,
			device&,
			command_queue&,
			std::size_t frame_count,
			std::span<const format> formats
		);
	private:
		context_options _context_options = context_options::none; ///< Context options.

		/// Initializes all fields of this struct.
		explicit context(context_options opts) : _context_options(opts) {
		}
	};

	/// The DXC compiler.
	class shader_utility : private common::dxc_compiler {
	protected:
		/// Holds a \p IRObject containing the compiled shader.
		struct compilation_result : private common::dxc_compiler::compilation_result {
			friend shader_utility;
		protected:
			/// Initializes this object to empty.
			compilation_result(std::nullptr_t) {
			}

			/// Calls the method in the base class.
			[[nodiscard]] bool succeeded() const {
				return common::dxc_compiler::compilation_result::succeeded();
			}
			/// Calls the method in the base class.
			[[nodiscard]] std::u8string_view get_compiler_output() {
				return common::dxc_compiler::compilation_result::get_compiler_output();
			}
			/// Calls the method in the base class.
			[[nodiscard]] std::span<const std::byte> get_compiled_binary() {
				return common::dxc_compiler::compilation_result::get_compiled_binary();
			}
		private:
			/// Initializes the base class object.
			explicit compilation_result(common::dxc_compiler::compilation_result result) :
				common::dxc_compiler::compilation_result(std::move(result)) {
			}
		};

		/// Creates a shader utility object.
		[[nodiscard]] static shader_utility create();

		/// Loads reflection data using DXC.
		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte> data) {
			return shader_reflection(dxc_compiler::load_shader_reflection(data));
		}
		/// Loads reflection data using DXC.
		[[nodiscard]] shader_library_reflection load_shader_library_reflection(std::span<const std::byte> data) {
			return shader_library_reflection(dxc_compiler::load_shader_library_reflection(data));
		}
		/// Compiles the given code using DXC.
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8,
			shader_stage,
			std::u8string_view entry,
			const std::filesystem::path &shader_path,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		);
		/// Compiles the given code using DXC.
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code_utf8,
			const std::filesystem::path &shader_path,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		);
	private:
		/// Initializes the base class object.
		shader_utility() : dxc_compiler(nullptr) {
		}
	};
}
