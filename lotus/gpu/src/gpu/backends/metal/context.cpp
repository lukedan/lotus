#include "lotus/gpu/backends/metal/context.h"

/// \file
/// Metal device management and creation code.

#include <Metal/Metal.hpp>

#include "lotus/gpu/backends/metal/details.h"

namespace lotus::gpu::backends::metal {
	context context::create(context_options opts, _details::debug_message_callback) {
		// TODO honor the options and debug callback?
		return context(opts);
	}

	std::vector<adapter> context::get_all_adapters() const {
		NS::SharedPtr<NS::Array> arr = NS::TransferPtr(MTL::CopyAllDevices());

		const auto count = static_cast<std::size_t>(arr->count());
		std::vector<adapter> result;
		result.reserve(count);
		for (std::size_t i = 0; i < count; ++i) {
			result.emplace_back(adapter(NS::RetainPtr(arr->object<MTL::Device>(i)), _context_options));
		}
		return result;
	}


	shader_utility shader_utility::create() {
		return shader_utility();
	}

	/// Additional arguments supplied to the compiler.
	constexpr static LPCWSTR _additional_args[] = { L"-Ges", L"-Zi", L"-Zpr" };

	shader_utility::compilation_result shader_utility::compile_shader(
		std::span<const std::byte> code_utf8,
		shader_stage stage,
		std::u8string_view entry,
		const std::filesystem::path &shader_path,
		std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		return compilation_result(dxc_compiler::compile_shader(
			code_utf8, stage, entry, shader_path, include_paths, defines, _additional_args
		));
	}

	shader_utility::compilation_result shader_utility::compile_shader_library(
		std::span<const std::byte> code_utf8,
		const std::filesystem::path &shader_path,
		std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		return compilation_result(dxc_compiler::compile_shader_library(
			code_utf8, shader_path, include_paths, defines, _additional_args
		));
	}

}
