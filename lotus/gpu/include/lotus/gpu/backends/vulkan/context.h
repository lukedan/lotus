#pragma once

/// \file
/// A Vulkan context.

#include "lotus/memory/stack_allocator.h"
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
		using debug_message_id = _details::debug_message_id; ///< Debug message ID type.

		/// Calls \p vk::createInstanceUnique().
		[[nodiscard]] static context create(context_options, _details::debug_message_callback);

		/// Calls \p vk::Instance::enumeratePhysicalDevices().
		[[nodiscard]] std::vector<adapter> get_all_adapters() const;

		/// Creates a platform-specific surface for the window, then creates a swapchain using
		/// \p createSwapchainKHRUnique().
		[[nodiscard]] std::pair<swap_chain, format> create_swap_chain_for_window(
			system::window&, device&, command_queue&, usize frame_count, std::span<const format>
		);
	private:
		/// Initializes the context.
		context(vk::UniqueInstance, context_options, _details::debug_message_callback);

		vk::UniqueInstance _instance; ///< The vulkan instance.
		vk::detail::DispatchLoaderDynamic _dispatch_loader; ///< Function pointer for extensions.
		vk::DebugReportCallbackEXT _debug_callback; ///< The debug callback.
		context_options _options = context_options::none; ///< Context options.
		std::unique_ptr<_details::debug_message_callback> _debug_callback_func; ///< Debug callback.
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
		/// Creates a new \p spv_reflect::ShaderModule object from the given code.
		[[nodiscard]] shader_library_reflection load_shader_library_reflection(std::span<const std::byte>);

		/// Compiles the given shader using the currently selected compiler.
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code, shader_stage, std::u8string_view entry,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		);
		/// Compiles the given shader library using the currently selected compiler.
		[[nodiscard]] compilation_result compile_shader_library(
			std::span<const std::byte> code,
			const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
			std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
		);
	private:
		backends::common::dxc_compiler _compiler = nullptr; ///< The compiler.
	};
}
