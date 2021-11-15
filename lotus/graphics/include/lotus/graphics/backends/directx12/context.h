#pragma once

/// \file
/// DirectX 12 backend.

#include <utility>
#include <filesystem>

#include <dxgi1_6.h>
#include <directx/d3d12.h>
#include <dxcapi.h>

#include "lotus/system/platforms/windows/window.h"
#include "lotus/graphics/common.h"
#include "details.h"
#include "device.h"

namespace lotus::graphics::backends::directx12 {
	/// A \p IDXGIFactory5 to access the DirectX 12 library.
	class context {
	protected:
		/// Initializes the DXGI factory.
		[[nodiscard]] static context create();

		/// Enumerates the list of adapters using .
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
		/// Contains a \p IDxcResult.
		struct compilation_result {
			friend shader_utility;
		protected:
			/// Returns whether the result of \p IDxcResult::GetStatus() is success.
			[[nodiscard]] bool succeeded() const;
			/// Caches \ref _output if necessary, and returns it.
			[[nodiscard]] std::u8string_view get_compiler_output();
			/// Caches \ref _binary if necessary, and returns it.
			[[nodiscard]] std::span<const std::byte> get_compiled_binary();
		private:
			_details::com_ptr<IDxcResult> _result; ///< Result.
			_details::com_ptr<IDxcBlob> _binary; ///< Cached compiled binary.
			_details::com_ptr<IDxcBlobUtf8> _messages; ///< Cached compiler output.
		};

		/// Does nothing - everything is lazy initialized.
		[[nodiscard]] static shader_utility create();

		/// Calls \p IDxcUtils::CreateReflection().
		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte>);
		/// Creates a reflection object for the compiled shader.
		[[nodiscard]] shader_reflection load_shader_reflection(compilation_result&);
		/// Calls \p IDxcCompiler3::Compile().
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte>, shader_stage, std::u8string_view entry_point,
			std::span<const std::filesystem::path> include_paths
		);
	private:
		_details::com_ptr<IDxcUtils> _dxc_utils; ///< Lazy-initialized DXC library handle.
		_details::com_ptr<IDxcCompiler3> _dxc_compiler; ///< Lazy-initialized DXC compiler.
		/// Lazy-initialized default DXC include handler.
		_details::com_ptr<IDxcIncludeHandler> _dxc_include_handler;

		/// Initializes \ref _dxc_utils if necessary, and returns it.
		[[nodiscard]] IDxcUtils &_utils();
		/// Initializes \ref _dxc_compiler if necessary, and returns it.
		[[nodiscard]] IDxcCompiler3 &_compiler();
		/// Initializes \ref _dxc_include_handler if necessary, and returns it.
		[[nodiscard]] IDxcIncludeHandler &_include_handler();
	};
}
