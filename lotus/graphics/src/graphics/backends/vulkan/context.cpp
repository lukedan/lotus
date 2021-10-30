#include "lotus/graphics/backends/vulkan/context.h"

/// \file
/// Implementation of Vulkan contexts.

#include <WinUser.h>

#include "lotus/graphics/backends/vulkan/details.h"
#include "lotus/system/common.h"

namespace lotus::graphics::backends::vulkan {
	context::~context() {
		if (_instance) {
			// TODO allocator
			_instance->destroyDebugReportCallbackEXT(_debug_callback, nullptr, _dispatch_loader);
		}
	}

	context context::create() {
		std::array enabled_layers = {
			"VK_LAYER_KHRONOS_validation"
		};
		std::array enabled_extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#ifdef WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
		};
		vk::ApplicationInfo app_info;
		app_info
			.setApiVersion(VK_API_VERSION_1_2)
			.setPEngineName("Lotus");
		vk::InstanceCreateInfo create_info;
		create_info
			.setPApplicationInfo(&app_info)
			.setPEnabledLayerNames(enabled_layers)
			.setPEnabledExtensionNames(enabled_extensions);
		// TODO allocator
		return context(_details::unwrap(vk::createInstanceUnique(create_info)));
	}

	context::context(vk::UniqueInstance inst) : _instance(std::move(inst)) {
		_dispatch_loader.init(_instance.get(), vkGetInstanceProcAddr);

		vk::DebugReportCallbackCreateInfoEXT debug_callback_info;
		debug_callback_info
			.setFlags(
				vk::DebugReportFlagBitsEXT::eError |
				vk::DebugReportFlagBitsEXT::eWarning |
				vk::DebugReportFlagBitsEXT::ePerformanceWarning
			)
			.setPfnCallback(
				[](
					VkDebugReportFlagsEXT /*flags*/,
					VkDebugReportObjectTypeEXT /*object_type*/,
					uint64_t /*object*/,
					size_t /*location*/,
					int32_t /*message_code*/,
					const char */*layer_prefix*/,
					const char *message,
					void */*user_data*/
				) -> VkBool32 {
					if (
						std::strstr(message, "VUID-VkShaderModuleCreateInfo-pCode-04147")/* || // ignore "unsupported SPIRV extension"
						std::strstr(message, "VUID-VkGraphicsPipelineCreateInfo-layout-00756")*/
					) {
						return false;
					}
					std::cerr << message << "\n\n";
					return false;
				}
			);
		// TODO allocator
		_debug_callback = _details::unwrap(_instance->createDebugReportCallbackEXT(
			debug_callback_info, nullptr, _dispatch_loader
		));
	}

	swap_chain context::create_swap_chain_for_window(
		system::window &wnd, device &dev, command_queue&, std::size_t frame_count, format fmt
	) {
		swap_chain result = nullptr;

		auto bookmark = stack_allocator::for_this_thread().bookmark();

#ifdef WIN32
		vk::Win32SurfaceCreateInfoKHR surface_info;
		auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(wnd.get_native_handle(), GWLP_HINSTANCE));
		system::platforms::windows::_details::assert_win32(instance);
		surface_info
			.setHinstance(instance)
			.setHwnd(wnd.get_native_handle());
		// TODO allocator
		result._surface = _details::unwrap(_instance->createWin32SurfaceKHRUnique(surface_info));
#endif

		assert(_details::unwrap(dev._physical_device.getSurfaceSupportKHR(
			dev._graphics_compute_queue_family_index, result._surface.get()
		)));
		auto capabilities = _details::unwrap(dev._physical_device.getSurfaceCapabilitiesKHR(result._surface.get()));
		auto formats_alloc = bookmark.create_std_allocator<vk::SurfaceFormatKHR>();
		auto formats = _details::unwrap(dev._physical_device.getSurfaceFormatsKHR(
			result._surface.get(), formats_alloc
		));

		vk::SwapchainCreateInfoKHR info;
		cvec2s size = wnd.get_size();
		info
			.setSurface(result._surface.get())
			.setMinImageCount(static_cast<std::uint32_t>(frame_count))
			.setImageFormat(_details::conversions::for_format(fmt)) // HACK
			.setImageColorSpace(formats[0].colorSpace/*vk::ColorSpaceKHR::eSrgbNonlinear*/)
			.setImageExtent(vk::Extent2D(static_cast<std::uint32_t>(size[0]), static_cast<std::uint32_t>(size[1])))
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
			.setImageSharingMode(vk::SharingMode::eExclusive)
			.setPresentMode(vk::PresentModeKHR::eMailbox)
			.setClipped(true);
		// TODO allocator
		result._swapchain = _details::unwrap(dev._device->createSwapchainKHRUnique(info));

		result._images = _details::unwrap(dev._device->getSwapchainImagesKHR(result._swapchain.get()));
		result._synchronization.resize(result._images.size(), nullptr);

		return result;
	}
}
