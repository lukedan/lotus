#pragma once

/// \file
/// A Vulkan context.

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
			auto bookmark = stack_allocator::scoped_bookmark::create();
			auto allocator = stack_allocator::for_this_thread().create_std_allocator<vk::PhysicalDevice>();
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
		[[nodiscard]] swap_chain create_swap_chain_for_window(
			system::window&, device&, command_queue&, std::size_t frame_count, format
		);
	private:
		/// Initializes the context.
		explicit context(vk::UniqueInstance);

		vk::UniqueInstance _instance; ///< The vulkan instance.
		vk::DispatchLoaderDynamic _dispatch_loader; ///< Function pointer for extensions.
		vk::DebugReportCallbackEXT _debug_callback; ///< The debug callback.
	};
}
