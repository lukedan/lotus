#pragma once

/// \file
/// A Vulkan context.

#include <Unknwn.h>
#include <dxcapi.h>

#include "lotus/utils/stack_allocator.h"
#include "lotus/system/window.h"
#include "details.h"
#include "device.h"
#include "frame_buffer.h"
#include "commands.h"

namespace lotus::graphics::backends::vulkan {
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
		[[nodiscard]] static context create();

		/// Calls \p vk::Instance::enumeratePhysicalDevices().
		template <typename Callback> void enumerate_adapters(Callback &&cb) {
			auto bookmark = stack_allocator::for_this_thread().bookmark();
			auto allocator = bookmark.create_std_allocator<vk::PhysicalDevice>();
			auto physical_devices = _details::unwrap(_instance->enumeratePhysicalDevices(allocator));
			for (const auto &dev : physical_devices) {
				adapter adap(dev, _dispatch_loader);
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
		explicit context(vk::UniqueInstance);

		vk::UniqueInstance _instance; ///< The vulkan instance.
		vk::DispatchLoaderDynamic _dispatch_loader; ///< Function pointer for extensions.
		vk::DebugReportCallbackEXT _debug_callback; ///< The debug callback.
	};

	/// Shader utilities using SPIRV-Reflect.
	class shader_utility {
	protected:
		// TODO find a way to reuse this code
		/// Shader compilation result using DXC.
		struct _compilation_result_dxc {
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

		using compilation_result = _compilation_result_dxc; ///< Compilation result type.

		/// Creates an instance of the utility object.
		[[nodiscard]] static shader_utility create();

		/// Creates a new \p spv_reflect::ShaderModule object from the given code.
		[[nodiscard]] shader_reflection load_shader_reflection(std::span<const std::byte>);
		/// Unlike DirectX 12, this is just an overload of the other function.
		[[nodiscard]] shader_reflection load_shader_reflection(_compilation_result_dxc&);

		/// Compiles the given shader using the currently selected compiler.
		[[nodiscard]] compilation_result compile_shader(
			std::span<const std::byte> code, shader_stage stage, std::u8string_view entry
		) {
			return _compile_shader_dxc(code, stage, entry);
		}
	private:
		_details::com_ptr<IDxcCompiler3> _dxc_compiler; ///< Lazy-initialized DXC compiler.

		/// Initializes \ref _dxc_compiler if necessary, and returns it.
		[[nodiscard]] IDxcCompiler3 &_compiler();

		/// Compiles a shader using DXC.
		_compilation_result_dxc _compile_shader_dxc(std::span<const std::byte>, shader_stage, std::u8string_view);
	};
}
