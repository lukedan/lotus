#include "lotus/graphics/backends/vulkan/context.h"

/// \file
/// Implementation of Vulkan contexts.

#include "lotus/graphics/backends/vulkan/details.h"
#include "lotus/system/common.h"

#ifdef WIN32
#	include <WinUser.h>
#	include "lotus/system/platforms/windows/details.h"
#endif

namespace lotus::graphics::backends::vulkan {
	context::~context() {
		if (_instance) {
			// TODO allocator
			_instance->destroyDebugReportCallbackEXT(_debug_callback, nullptr, _dispatch_loader);
		}
	}

	context context::create(context_options opt) {
		auto bookmark = stack_allocator::for_this_thread().bookmark();
		auto enabled_layers = bookmark.create_vector_array<const char*>();
		if (!is_empty(opt & context_options::enable_validation)) {
			enabled_layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}
		std::array enabled_extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#ifdef WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
		};
		vk::ApplicationInfo app_info;
		app_info
			.setApiVersion(VK_API_VERSION_1_3)
			.setPApplicationName("Lotus")
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
					VkDebugReportFlagsEXT flags,
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
					if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
						log().error<u8"{}">(message);
					} else if (
						flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
					) {
						log().warn<u8"{}">(message);
					} else {
						log().info<u8"{}">(message);
					}
					return false;
				}
			);
		// TODO allocator
		_debug_callback = _details::unwrap(_instance->createDebugReportCallbackEXT(
			debug_callback_info, nullptr, _dispatch_loader
		));
	}

	std::pair<swap_chain, format> context::create_swap_chain_for_window(
		system::window &wnd, device &dev, command_queue&, std::size_t frame_count, std::span<const format> formats
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
		auto allocator = bookmark.create_std_allocator<vk::SurfaceFormatKHR>();
		auto available_fmts = _details::unwrap(dev._physical_device.getSurfaceFormatsKHR(
			result._surface.get(), allocator
		));
		std::sort(
			available_fmts.begin(), available_fmts.end(),
			[](const vk::SurfaceFormatKHR &lhs, const vk::SurfaceFormatKHR &rhs) {
				if (lhs.format == rhs.format) {
					// make sure sRGB is considered first
					return
						lhs.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear &&
						rhs.colorSpace != vk::ColorSpaceKHR::eSrgbNonlinear;
				}
				return lhs.format < rhs.format;
			}
		);
		result._format = available_fmts[0];
		format result_format = format::none;
		for (auto fmt : formats) {
			vk::Format result_fmt = _details::conversions::to_format(fmt);
			auto it = std::lower_bound(
				available_fmts.begin(), available_fmts.end(), result_fmt,
				[](const vk::SurfaceFormatKHR &lhs, vk::Format rhs) {
					return lhs.format < rhs;
				}
			);
			if (it != available_fmts.end() && it->format == result_fmt) {
				result._format = *it;
				result_format = fmt;
				break;
			}
		}
		if (result_format == format::none) {
			result_format = _details::conversions::back_to_format(result._format.format);
		}

		vk::SwapchainCreateInfoKHR info;
		cvec2s size = wnd.get_size();
		info
			.setSurface(result._surface.get())
			.setMinImageCount(static_cast<std::uint32_t>(frame_count))
			.setImageFormat(result._format.format)
			.setImageColorSpace(result._format.colorSpace)
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

		return { std::move(result), result_format };
	}


	shader_utility shader_utility::create() {
		return shader_utility();
	}

	shader_reflection shader_utility::load_shader_reflection(std::span<const std::byte> code) {
		shader_reflection result = nullptr;
		result._reflection = spv_reflect::ShaderModule(code.size(), code.data());
		_details::assert_spv_reflect(result._reflection.GetResult());
		return result;
	}

	shader_reflection shader_utility::load_shader_reflection(compilation_result &res) {
		return load_shader_reflection(res.get_compiled_binary());
	}
}
