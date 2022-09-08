#pragma once

/// \file
/// DirectX shader compiler interface.

#include <filesystem>
#include <span>

#include <winerror.h>

#ifdef _WIN32
#	include <atlbase.h>
#endif
#include <dxcapi.h>

#include "lotus/logging.h"
#include "lotus/gpu/common.h"

namespace lotus::gpu::backends::common {
	namespace _details {
		/// COM pointers.
		template <typename T> using com_ptr = CComPtr<T>;

		/// Checks that the given \p HRESULT indicates success.
		inline void assert_dx(HRESULT hr) {
			if (hr != S_OK) {
				log().error<u8"DirectX error {}">(hr);
				std::abort();
			}
		}
	}

	/// DXC compiler.
	struct dxc_compiler {
	public:
		/// Contains a \p IDxcResult.
		struct compilation_result {
			friend dxc_compiler;
		public:
			/// Returns whether the result of \p IDxcResult::GetStatus() is success.
			[[nodiscard]] bool succeeded() const;
			/// Caches \ref _output if necessary, and returns it.
			[[nodiscard]] std::u8string_view get_compiler_output();
			/// Caches \ref _binary if necessary, and returns it.
			[[nodiscard]] std::span<const std::byte> get_compiled_binary();

			/// Returns a reference to the raw \p IDxcResult.
			[[nodiscard]] IDxcResult &get_result() const {
				return *_result;
			}
		private:
			_details::com_ptr<IDxcResult> _result; ///< Result.
			_details::com_ptr<IDxcBlob> _binary; ///< Cached compiled binary.
			_details::com_ptr<IDxcBlobUtf8> _messages; ///< Cached compiler output.
		};

		/// No initialization.
		dxc_compiler(std::nullptr_t) {
		}

		/// Calls \p IDxcCompiler3::Compile().
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte>, shader_stage, std::u8string_view entry_point,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines,
			std::span<const LPCWSTR> args
		);
		/// \overload
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code, shader_stage stage, std::u8string_view entry_point,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines,
			std::initializer_list<LPCWSTR> args
		) {
			return compile_shader(code, stage, entry_point, include_paths, defines, { args.begin(), args.end() });
		}

		/// Calls \p IDxcCompiler3::Compile().
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines,
			std::span<const LPCWSTR> args
		);
		/// \overload
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines,
			std::initializer_list<const LPCWSTR> args
		) {
			return compile_shader_library(code, include_paths, defines, { args.begin(), args.end() });
		}

		/// Initializes \ref _dxc_utils if necessary, and returns it.
		[[nodiscard]] IDxcUtils &get_utils();
		/// Initializes \ref _dxc_compiler if necessary, and returns it.
		[[nodiscard]] IDxcCompiler3 &get_compiler();
		/// Initializes \ref _dxc_include_handler if necessary, and returns it.
		[[nodiscard]] IDxcIncludeHandler &get_include_handler();
	private:
		_details::com_ptr<IDxcUtils> _dxc_utils; ///< Lazy-initialized DXC library handle.
		_details::com_ptr<IDxcCompiler3> _dxc_compiler; ///< Lazy-initialized DXC compiler.
		/// Lazy-initialized default DXC include handler.
		_details::com_ptr<IDxcIncludeHandler> _dxc_include_handler;

		/// Calls \p IDxcCompiler3::Compile().
		[[nodiscard]] compilation_result _do_compile_shader(
			std::span<const std::byte> code,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines,
			std::span<const LPCWSTR> args,
			std::initializer_list<LPCWSTR> extra_args_2
		);
	};
}
