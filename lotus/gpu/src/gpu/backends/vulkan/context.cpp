#include "lotus/gpu/backends/vulkan/context.h"

/// \file
/// Implementation of Vulkan contexts.

#include "lotus/gpu/backends/vulkan/details.h"
#include "lotus/system/common.h"

#ifdef WIN32
#	include <WinUser.h>
#	include "lotus/system/platforms/windows/details.h"
#endif

namespace lotus::gpu::backends::vulkan {
	context::~context() {
		if (_instance) {
			// TODO allocator
			_instance->destroyDebugReportCallbackEXT(_debug_callback, nullptr, _dispatch_loader);
		}
	}

	context context::create(context_options opt, _details::debug_message_callback debug_cb) {
		auto bookmark = get_scratch_bookmark();
		auto enabled_layers = bookmark.create_vector_array<const char*>();
		if (bit_mask::contains<context_options::enable_validation>(opt)) {
			enabled_layers.insert(enabled_layers.end(), {
				"VK_LAYER_KHRONOS_validation",
				/*"VK_LAYER_LUNARG_parameter_validation",*/
			});
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
		return context(_details::unwrap(vk::createInstanceUnique(create_info)), opt, std::move(debug_cb));
	}

	context::context(vk::UniqueInstance inst, context_options opt, _details::debug_message_callback debug_cb) :
		_instance(std::move(inst)),
		_options(opt),
		_debug_callback_func(std::make_unique<_details::debug_message_callback>(std::move(debug_cb))) {

		_dispatch_loader.init(static_cast<VkInstance>(_instance.get()), vkGetInstanceProcAddr);

		vk::DebugReportCallbackCreateInfoEXT debug_callback_info;
		debug_callback_info
			.setFlags(
				vk::DebugReportFlagBitsEXT::eInformation        |
				vk::DebugReportFlagBitsEXT::eWarning            |
				vk::DebugReportFlagBitsEXT::ePerformanceWarning |
				vk::DebugReportFlagBitsEXT::eError              |
				vk::DebugReportFlagBitsEXT::eDebug
			)
			.setPfnCallback(
				[](
					VkDebugReportFlagsEXT flags,
					VkDebugReportObjectTypeEXT,
					uint64_t /*object*/,
					size_t location,
					int32_t /*message_code*/,
					const char */*layer_prefix*/,
					const char *message,
					void *user_data
				) -> VkBool32 {
					auto &cb = *static_cast<_details::debug_message_callback*>(user_data);
					if (cb) {
						const auto vkFlags = static_cast<vk::DebugReportFlagsEXT>(flags);
						const std::u8string_view u8message(reinterpret_cast<const char8_t*>(message));
						// TODO: it turns out that location argument contains the message ID, not message code
						//       feels like a bug but it seems to have been like this forever
						const auto msg_id = static_cast<debug_message_id>(location);
						cb(_details::conversions::back_to_debug_message_severity(vkFlags), msg_id, u8message);
					}
					return false;
				}
			)
			.setPUserData(_debug_callback_func.get());
		// TODO allocator
		_debug_callback = _details::unwrap(_instance->createDebugReportCallbackEXT(
			debug_callback_info, nullptr, _dispatch_loader
		));
	}

	std::pair<swap_chain, format> context::create_swap_chain_for_window(
		system::window &wnd, device &dev, command_queue&, std::size_t frame_count, std::span<const format> formats
	) {
		swap_chain result = nullptr;

		auto bookmark = get_scratch_bookmark();

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

		crash_if(!_details::unwrap(dev._physical_device.getSurfaceSupportKHR(
			dev._queue_family_props[queue_family::graphics].index, result._surface.get()
		)));
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
		auto reflection = std::make_shared<spv_reflect::ShaderModule>(code.size(), code.data()); // TODO allocator
		_details::assert_spv_reflect(reflection->GetResult());
		assert(reflection->GetEntryPointCount() == 1);
		return shader_reflection(std::move(reflection), 0);
	}

	shader_library_reflection shader_utility::load_shader_library_reflection(std::span<const std::byte> code) {
		shader_library_reflection result = nullptr;
		result._reflection = std::make_shared<spv_reflect::ShaderModule>(code.size(), code.data()); // TODO allocator
		_details::assert_spv_reflect(result._reflection->GetResult());
		return result;
	}

	/// Additional args supplied to the shader compiler for SPIR-V.
	static const LPCWSTR _additional_args[] = {
		L"-Ges",
		L"-Zi",
		L"-Zpr",
		L"-spirv",
		L"-fspv-reflect",
		L"-fspv-target-env=vulkan1.2",
		L"-fvk-use-dx-layout",
	};
	shader_utility::compilation_result shader_utility::compile_shader(
		std::span<const std::byte> code, shader_stage stage, std::u8string_view entry,
		const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		auto bookmark = get_scratch_bookmark();
		auto extra_args = bookmark.create_vector_array<LPCWSTR>();
		extra_args.insert(extra_args.end(), std::begin(_additional_args), std::end(_additional_args));
		if (stage == shader_stage::vertex_shader || stage == shader_stage::geometry_shader) {
			extra_args.emplace_back(L"-fvk-invert-y");
		}
		return _compiler.compile_shader(code, stage, entry, shader_path, include_paths, defines, extra_args);
	}

	shader_utility::compilation_result shader_utility::compile_shader_library(
		std::span<const std::byte> code,
		const std::filesystem::path &shader_path, std::span<const std::filesystem::path> include_paths,
		std::span<const std::pair<std::u8string_view, std::u8string_view>> defines
	) {
		auto bookmark = get_scratch_bookmark();
		auto extra_args = bookmark.create_vector_array<LPCWSTR>();
		extra_args.insert(extra_args.end(), std::begin(_additional_args), std::end(_additional_args));
		return _compiler.compile_shader_library(code, shader_path, include_paths, defines, extra_args);
	}
}
