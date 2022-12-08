#include "lotus/gpu/backends/common/dxc.h"

/// \file
/// Implementation of DXC functions.

#include "lotus/memory/stack_allocator.h"
#include "lotus/utils/strings.h"

namespace lotus::gpu::backends::common {
	bool dxc_compiler::compilation_result::succeeded() const {
		HRESULT stat;
		_details::assert_dx(_result->GetStatus(&stat));
		return stat == S_OK;
	}

	std::u8string_view dxc_compiler::compilation_result::get_compiler_output() {
		if (!_messages) {
			_details::assert_dx(_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&_messages), nullptr));
		}
		const auto *str = static_cast<const char8_t*>(_messages->GetBufferPointer());
		return std::u8string_view(str, _messages->GetBufferSize());
	}

	std::span<const std::byte> dxc_compiler::compilation_result::get_compiled_binary() {
		if (!_binary) {
			_details::assert_dx(_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&_binary), nullptr));
		}
		return std::span(static_cast<const std::byte*>(_binary->GetBufferPointer()), _binary->GetBufferSize());
	}


	/// Converts a UTF-8 string to a wide string.
	[[nodiscard]] static memory::stack_allocator::string_type<WCHAR> _u8string_to_wstring(
		memory::stack_allocator::scoped_bookmark &bookmark, std::u8string_view view
	) {
		// TODO this assumes the entry point doesn't contain chars above 128
		return bookmark.create_string<WCHAR>(view.begin(), view.end());
	}
	dxc_compiler::compilation_result dxc_compiler::compile_shader(
		std::span<const std::byte> code, shader_stage stage, std::u8string_view entry,
		const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines,
		std::span<const LPCWSTR> extra_args
	) {
		constexpr static enum_mapping<shader_stage, std::string_view> stage_names{
			std::pair(shader_stage::all,                   "INVALID"),
			std::pair(shader_stage::vertex_shader,         "vs" ),
			std::pair(shader_stage::geometry_shader,       "gs" ),
			std::pair(shader_stage::pixel_shader,          "ps" ),
			std::pair(shader_stage::compute_shader,        "cs" ),
			std::pair(shader_stage::callable_shader,       "INVALID"),
			std::pair(shader_stage::ray_generation_shader, "INVALID"),
			std::pair(shader_stage::intersection_shader,   "INVALID"),
			std::pair(shader_stage::any_hit_shader,        "INVALID"),
			std::pair(shader_stage::closest_hit_shader,    "INVALID"),
			std::pair(shader_stage::miss_shader,           "INVALID"),
		};
		using _wstring = memory::stack_allocator::string_type<WCHAR>;

		auto bookmark = get_scratch_bookmark();

		auto entry_wstr = _u8string_to_wstring(bookmark, entry);

		char profile_ascii[10];
		auto fmt_result = std::format_to_n(
			profile_ascii, std::size(profile_ascii) - 1, "{}_{}_{}", stage_names[stage], 6, 5
		);
		assert(static_cast<std::size_t>(fmt_result.size) + 1 < std::size(profile_ascii));
		profile_ascii[fmt_result.size] = L'\0';
		auto profile = _u8string_to_wstring(bookmark, reinterpret_cast<const char8_t*>(profile_ascii));

		return _do_compile_shader(
			code, shader_path, include_paths, defines, extra_args,
			{
				L"-E", entry_wstr.c_str(),
				L"-T", profile.c_str(),
				L"-Zi",
				L"-Qembed_debug",
			}
		);
	}

	dxc_compiler::compilation_result dxc_compiler::compile_shader_library(
		std::span<const std::byte> code,
		const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines,
		std::span<const LPCWSTR> args
	) {
		return _do_compile_shader(
			code, shader_path, include_paths, defines, args,
			{
				L"-T", L"lib_6_3",
				L"-Zi",
				L"-Qembed_debug",
			}
		);
	}

	IDxcUtils &dxc_compiler::get_utils() {
		if (!_dxc_utils) {
			_details::assert_dx(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&_dxc_utils)));
		}
		return *_dxc_utils;
	}

	IDxcCompiler3 &dxc_compiler::get_compiler() {
		if (!_dxc_compiler) {
			_details::assert_dx(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&_dxc_compiler)));
		}
		return *_dxc_compiler;
	}

	IDxcIncludeHandler &dxc_compiler::get_include_handler() {
		if (!_dxc_include_handler) {
			_details::assert_dx(get_utils().CreateDefaultIncludeHandler(&_dxc_include_handler));
		}
		return *_dxc_include_handler;
	}

	dxc_compiler::compilation_result dxc_compiler::_do_compile_shader(
		std::span<const std::byte> code,
		const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines,
		std::span<const LPCWSTR> extra_args,
		std::initializer_list<LPCWSTR> extra_args_2
	) {
		using _wstring = memory::stack_allocator::string_type<WCHAR>;

		auto bookmark = get_scratch_bookmark();

		auto includes = bookmark.create_reserved_vector_array<_wstring>(include_paths.size());
		for (const auto &p : include_paths) {
			includes.emplace_back(bookmark.create_string<WCHAR>(p.wstring())); // TODO allocator?
		}

		auto defs = bookmark.create_reserved_vector_array<_wstring>(defines.size());
		for (const auto &def : defines) {
			auto str = bookmark.create_string();
			if (def.second.empty()) {
				std::format_to(std::back_inserter(str), "-D{}", string::to_generic(def.first));
			} else {
				std::format_to(
					std::back_inserter(str), "-D{}={}",
					string::to_generic(def.first), string::to_generic(def.second)
				);
			}
			defs.emplace_back(_u8string_to_wstring(bookmark, string::assume_utf8(str)));
		}

		auto args = bookmark.create_vector_array<LPCWSTR>();
		auto shader_path_str = shader_path.wstring();
		if (!shader_path.empty()) {
			args.emplace_back(shader_path_str.c_str());
		}
		args.insert(args.end(), extra_args_2.begin(), extra_args_2.end());
		for (const auto &inc : includes) {
			args.insert(args.end(), { L"-I", inc.c_str() });
		}
		for (const auto arg : extra_args) {
			args.emplace_back(arg);
		}
		for (const auto &def : defs) {
			args.emplace_back(def.c_str());
		}

		DxcBuffer buffer;
		buffer.Ptr      = code.data();
		buffer.Size     = code.size();
		buffer.Encoding = DXC_CP_UTF8;
		compilation_result result;
		_details::assert_dx(get_compiler().Compile(
			&buffer, args.data(), static_cast<UINT32>(args.size()),
			&get_include_handler(), IID_PPV_ARGS(&result._result)
		));
		return result;
	}
}
