#include "lotus/gpu/backends/metal/context.h"

/// \file
/// Metal device management and creation code.

#include <Metal/Metal.hpp>

#include "lotus/gpu/backends/metal/details.h"

namespace lotus::gpu::backends::metal {
	/// Converts a \ref shader_stage to a \p IRShaderStage.
	[[nodiscard]] IRShaderStage _to_shader_stage(shader_stage stage) {
		constexpr static enums::sequential_mapping<shader_stage, IRShaderStage> table{
			std::pair(shader_stage::all,                   IRShaderStageInvalid      ),
			std::pair(shader_stage::vertex_shader,         IRShaderStageVertex       ),
			std::pair(shader_stage::geometry_shader,       IRShaderStageGeometry     ),
			std::pair(shader_stage::pixel_shader,          IRShaderStageFragment     ),
			std::pair(shader_stage::compute_shader,        IRShaderStageCompute      ),
			std::pair(shader_stage::callable_shader,       IRShaderStageCallable     ),
			std::pair(shader_stage::ray_generation_shader, IRShaderStageRayGeneration),
			std::pair(shader_stage::intersection_shader,   IRShaderStageIntersection ),
			std::pair(shader_stage::any_hit_shader,        IRShaderStageAnyHit       ),
			std::pair(shader_stage::closest_hit_shader,    IRShaderStageClosestHit   ),
			std::pair(shader_stage::miss_shader,           IRShaderStageMiss         ),
		};
		return table[stage];
	}


	context context::create(context_options, _details::debug_message_callback) {
		// TODO honor the options and debug callback?
		return context();
	}

	std::vector<adapter> context::get_all_adapters() const {
		NS::SharedPtr<NS::Array> arr = NS::TransferPtr(MTL::CopyAllDevices());

		const auto count = static_cast<std::size_t>(arr->count());
		std::vector<adapter> result;
		result.reserve(count);
		for (std::size_t i = 0; i < count; ++i) {
			result.emplace_back(adapter(NS::RetainPtr(arr->object<MTL::Device>(i))));
		}
		return result;
	}


	shader_utility::compilation_result::~compilation_result() {
		if (_ir) {
			IRObjectDestroy(_ir);
		}
	}

	shader_utility::compilation_result::compilation_result(compilation_result &&src) :
		_compiler_output(std::move(src._compiler_output)),
		_ir(std::exchange(src._ir, nullptr)),
		_stage(std::exchange(src._stage, shader_stage::num_enumerators)) {
	}

	shader_utility::compilation_result &shader_utility::compilation_result::operator=(compilation_result &&src) {
		if (&src != this) {
			if (_ir) {
				IRObjectDestroy(_ir);
			}
			_compiler_output = std::move(src._compiler_output);
			_ir              = std::exchange(src._ir, nullptr);
			_stage           = std::exchange(src._stage, shader_stage::num_enumerators);
		}
		return *this;
	}

	std::span<const std::byte> shader_utility::compilation_result::get_compiled_binary() {
		std::size_t size = 0;
		const void *ptr = nullptr;

		IRMetalLibBinary *metal_lib = IRMetalLibBinaryCreate();
		IRObjectGetMetalLibBinary(_ir, _to_shader_stage(_stage), metal_lib);
		dispatch_data_t data = IRMetalLibGetBytecodeData(metal_lib);
		dispatch_data_t data_map = dispatch_data_create_map(data, &ptr, &size);
		// TODO is this ok?
		dispatch_release(data_map);
		dispatch_release(data);
		/*IRMetalLibBinaryDestroy(metal_lib);*/ // TODO doing this somehow frees the IR itself?

		return std::span<const std::byte>(static_cast<const std::byte*>(ptr), size);
	}


	shader_utility shader_utility::create() {
		return shader_utility();
	}

	/// Additional arguments supplied to the compiler.
	static const LPCWSTR _additional_args[] = { L"-Ges", L"-Zi", L"-Zpr" };
	shader_utility::compilation_result shader_utility::compile_shader(
		std::span<const std::byte> code_utf8,
		shader_stage stage,
		std::u8string_view entry,
		const std::filesystem::path &shader_path,
		std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		compilation_result result = nullptr;
		result._stage = stage;

		common::dxc_compiler::compilation_result result_dx = _dxc_compiler.compile_shader(
			code_utf8, stage, entry, shader_path, include_paths, defines, _additional_args
		);

		result._compiler_output = result_dx.get_compiler_output();
		if (!result_dx.succeeded()) {
			return result;
		}

		// get compiled DXIL
		std::span<const std::byte> dxil_bin = result_dx.get_compiled_binary();
		IRObject *dxil = IRObjectCreateFromDXIL(
			reinterpret_cast<const std::uint8_t*>(dxil_bin.data()), dxil_bin.size(), IRBytecodeOwnershipNone
		);

		IRCompiler *compiler = IRCompilerCreate();
		std::string entry_str(string::to_generic(entry));
		IRCompilerSetEntryPointName(compiler, entry_str.c_str());

		// convert
		IRError *error = nullptr;
		IRObject *out_ir = IRCompilerAllocCompileAndLink(compiler, entry_str.c_str(), dxil, &error);
		if (error) {
			// TODO handle error
			IRErrorDestroy(error);
			return result;
		}
		result._ir = out_ir;

		IRObjectDestroy(dxil);
		IRCompilerDestroy(compiler);

		return result;
	}
}
