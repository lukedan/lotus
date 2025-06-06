#pragma once

/// \file
/// DirectX 12 backend.

#include <utility>
#include <filesystem>

#include <dxgi1_6.h>

#include "lotus/system/platforms/windows/window.h"
#include "lotus/gpu/backends/common/dxc.h"
#include "lotus/gpu/common.h"
#include "details.h"
#include "device.h"

namespace lotus::gpu::backends::directx12 {
	/// A \p IDXGIFactory5 to access the DirectX 12 library.
	class context {
	protected:
		using debug_message_id = _details::debug_message_id; ///< Debug message ID type.

		/// Initializes the DXGI factory.
		[[nodiscard]] static context create(context_options, _details::debug_message_callback);

		/// Enumerates the list of adapters using \p IDXGIFactory6::EnumAdapterByGpuPreference().
		[[nodiscard]] std::vector<adapter> get_all_adapters() const;
		/// Calls \p CreateSwapChainForHwnd to create a swap chain.
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::platforms::windows::window&, device&, command_queue&, usize, std::span<const format>
		);
	private:
		_details::com_ptr<IDXGIFactory6> _dxgi_factory; ///< The DXGI factory.
		std::unique_ptr<_details::debug_message_callback> _debug_message_callback; ///< The debug message callback.
	};

	/// Contains DXC classes.
	class shader_utility {
	protected:
		/// Compilation result interface.
		struct compilation_result : protected backends::common::dxc_compiler::compilation_result {
			friend shader_utility;
		protected:
			using _base = backends::common::dxc_compiler::compilation_result; ///< Base class.

			/// Returns whether compilation succeeded.
			[[nodiscard]] bool succeeded() const {
				return _base::succeeded();
			}
			/// Returns compiler messages.
			[[nodiscard]] std::u8string_view get_compiler_output() {
				return _base::get_compiler_output();
			}
			/// Returns compiled binary.
			[[nodiscard]] std::span<const std::byte> get_compiled_binary() {
				return _base::get_compiled_binary();
			}
		private:
			/// Initializes the base object.
			compilation_result(_base &&b) : _base(std::move(b)) {
			}
		};

		/// Does nothing - everything is lazy initialized.
		[[nodiscard]] static shader_utility create();

		/// Compiles a shader.
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code_utf8, shader_stage stage, std::u8string_view entry,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		);
		/// Compiles a shader library.
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code_utf8,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		);

		/// Calls \p IDxcUtils::CreateReflection().
		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte>);
		/// Calls \p IDxcUtils::CreateReflection().
		[[nodiscard]] shader_library_reflection load_shader_library_reflection(std::span<const std::byte>);
	private:
		backends::common::dxc_compiler _compiler = nullptr; ///< Interface to the DXC compiler.
	};
}
