#pragma once

/// \file
/// DirectX 12 backend.

#include <utility>
#include <filesystem>

#include <dxgi1_6.h>
#include <directx/d3d12.h>
#include <dxcapi.h>

#include "lotus/system/platforms/windows/window.h"
#include "lotus/gpu/backends/common/dxc.h"
#include "lotus/gpu/common.h"
#include "details.h"
#include "device.h"

namespace lotus::gpu::backends::directx12 {
	/// A \p IDXGIFactory5 to access the DirectX 12 library.
	class context {
	protected:
		/// Initializes the DXGI factory.
		[[nodiscard]] static context create(context_options);

		/// Enumerates the list of adapters using \p IDXGIFactory6::EnumAdapterByGpuPreference().
		template <typename Callback> void enumerate_adapters(Callback &&cb) {
			for (UINT i = 0; ; ++i) {
				adapter adap = nullptr;
				auto result = _dxgi_factory->EnumAdapterByGpuPreference(
					i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adap._adapter)
				);
				if (result == DXGI_ERROR_NOT_FOUND) {
					break;
				}
				if (!cb(std::move(adap))) {
					break;
				}
			}
		}
		/// Calls \p CreateSwapChainForHwnd to create a swap chain.
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::platforms::windows::window&, device&, command_queue&, std::size_t, std::span<const format>
		);
	private:
		_details::com_ptr<IDXGIFactory6> _dxgi_factory; ///< The DXGI factory.
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
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			return _compiler.compile_shader(
				code_utf8, stage, entry, include_paths, defines, { L"-Ges", L"-Zi", L"-Zpr", L"-no-legacy-cbuf-layout" }
			);
		}

		/// Calls \p IDxcUtils::CreateReflection().
		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte>);
		/// Creates a reflection object for the compiled shader.
		[[nodiscard]] shader_reflection load_shader_reflection(compilation_result&);
	private:
		backends::common::dxc_compiler _compiler = nullptr; ///< Interface to the DXC compiler.
	};
}
