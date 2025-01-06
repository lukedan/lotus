#pragma once

/// \file
/// Metal contexts.

#include <filesystem>
#include <vector>
#include <span>

#include <metal_irconverter/metal_irconverter.h>

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
	};

	// TODO
	class shader_utility {
	protected:
		/// Holds a \p IRObject containing the compiled shader.
		struct compilation_result {
			friend shader_utility;
		public:
			/// Destroys the IR object if necessary.
			~compilation_result();
		protected:
			/// Move construction.
			compilation_result(compilation_result&&);
			/// No copy construction.
			compilation_result(const compilation_result&) = delete;
			/// Move assignment.
			compilation_result &operator=(compilation_result&&);
			/// No copy assignment.
			compilation_result &operator=(const compilation_result&) = delete;

			/// Returns \p true if \ref _ir is not \p nullptr.
			[[nodiscard]] bool succeeded() const {
				return _ir != nullptr;
			}
			/// \return \ref _compiler_output.
			[[nodiscard]] std::u8string_view get_compiler_output() {
				return _compiler_output;
			}
			/// Retrieves the compiled IR from the \p IRObject.
			[[nodiscard]] std::span<const std::byte> get_compiled_binary();
		private:
			std::u8string _compiler_output; ///< Combined compiler output of DXC and the Metal shader converter.
			IRObject *_ir = nullptr; ///< Output Metal IR.
			shader_stage _stage = shader_stage::num_enumerators; ///< The shader stage.

			/// Initializes this object to empty.
			compilation_result(std::nullptr_t) {
			}
		};

		/// Creates a shader utility object.
		[[nodiscard]] static shader_utility create();

		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte> data) {
			// TODO
		}
		[[nodiscard]] shader_library_reflection load_shader_library_reflection(std::span<const std::byte> data) {
			// TODO
		}
		/// Compiles the given code using DXC, then converts it to Metal IR using Metal shader converter.
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8,
			shader_stage,
			std::u8string_view entry,
			const std::filesystem::path &shader_path,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		);
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code_utf8,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			// TODO
		}
	private:
		common::dxc_compiler _dxc_compiler = nullptr; ///< The DXC compiler.
	};
}
