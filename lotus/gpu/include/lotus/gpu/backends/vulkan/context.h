#pragma once

/// \file
/// A Vulkan context.

#include <Unknwn.h>
#include <dxcapi.h>

#include "lotus/utils/stack_allocator.h"
#include "lotus/system/window.h"
#include "lotus/gpu/backends/common/dxc.h"
#include "details.h"
#include "device.h"
#include "frame_buffer.h"
#include "commands.h"

namespace lotus::gpu::backends::vulkan {
	/// Contains a \p vk::UniqueInstance.
	class context {
	public:
		/// Default move constructin.
		context(context&&) = default;
		/// No copy construction.
		context(const context&) = delete;
		/// No copy assignment.
		context &operator=(const context&) = delete;
		/// Destructor.
		~context();
	protected:
		/// Calls \p vk::createInstanceUnique().
		[[nodiscard]] static context create(context_options);

		/// Calls \p vk::Instance::enumeratePhysicalDevices().
		template <typename Callback> void enumerate_adapters(Callback &&cb) {
			auto bookmark = stack_allocator::for_this_thread().bookmark();
			auto allocator = bookmark.create_std_allocator<vk::PhysicalDevice>();
			auto physical_devices = _details::unwrap(_instance->enumeratePhysicalDevices(allocator));
			for (const auto &dev : physical_devices) {
				adapter adap(dev, _options, _dispatch_loader);
				if (!cb(adap)) {
					break;
				}
			}
		}

		/// Creates a platform-specific surface for the window, then creates a swapchain using
		/// \p createSwapchainKHRUnique().
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::window&, device&, command_queue&, std::size_t frame_count, std::span<const format>
		);
	private:
		/// Initializes the context.
		explicit context(vk::UniqueInstance, context_options);

		vk::UniqueInstance _instance; ///< The vulkan instance.
		vk::DispatchLoaderDynamic _dispatch_loader; ///< Function pointer for extensions.
		vk::DebugReportCallbackEXT _debug_callback; ///< The debug callback.
		context_options _options = context_options::none; ///< Context options.
	};

	/// Shader utilities using SPIRV-Reflect.
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

		/// Creates an instance of the utility object.
		[[nodiscard]] static shader_utility create();

		/// Creates a new \p spv_reflect::ShaderModule object from the given code.
		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte>);
		/// Unlike DirectX 12, this is just an overload of the other function.
		[[nodiscard]] shader_reflection load_shader_reflection(compilation_result&);

		/// Compiles the given shader using the currently selected compiler.
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code, shader_stage stage, std::u8string_view entry,
			std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		) {
			auto bookmark = stack_allocator::for_this_thread().bookmark();
			auto extra_args = bookmark.create_vector_array<LPCWSTR>();
			extra_args.insert(
				extra_args.end(),
				{
					L"-Ges",
					L"-Zi",
					L"-Zpr",
					L"-spirv",
					L"-fspv-reflect",
					L"-fspv-target-env=vulkan1.2",
					L"-fvk-use-dx-layout",
					L"-no-legacy-cbuf-layout"
				}
			);
			if (stage == shader_stage::vertex_shader || stage == shader_stage::geometry_shader) {
				extra_args.emplace_back(L"-fvk-invert-y");
			}
			return _compiler.compile_shader(code, stage, entry, include_paths, defines, extra_args);
		}
	private:
		backends::common::dxc_compiler _compiler = nullptr; ///< The compiler.
	};
}
