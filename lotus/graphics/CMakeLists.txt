set(LOTUS_GRAPHICS_USE_DIRECTX12 YES CACHE STRING "Whether to generate target for the DirectX 12 backend.")
set(LOTUS_GRAPHICS_USE_VULKAN YES CACHE STRING "Whether to generate target for the Vulkan backend.")

set(LOTUS_GRAPHICS_BACKEND_INCLUDE_NAME "LOTUS_GRAPHICS_BACKEND_INCLUDE")
set(LOTUS_GRAPHICS_COMMON_HEADERS
	"include/lotus/graphics/commands.h"
	"include/lotus/graphics/common.h"
	"include/lotus/graphics/context.h"
	"include/lotus/graphics/descriptors.h"
	"include/lotus/graphics/device.h"
	"include/lotus/graphics/frame_buffer.h"
	"include/lotus/graphics/pass.h"
	"include/lotus/graphics/pipeline.h"
	"include/lotus/graphics/resources.h"
	"include/lotus/graphics/synchronization.h")

# DirectX 12
if(WIN32 AND LOTUS_GRAPHICS_USE_DIRECTX12)
	add_library(lotus_graphics_directx12 STATIC)
	set_target_compile_options(lotus_graphics_directx12)

	add_subdirectory("thirdparty/DirectX-Headers/")

	target_include_directories(lotus_graphics_directx12 PUBLIC "include/")
	target_sources(lotus_graphics_directx12
		PUBLIC
			${LOTUS_GRAPHICS_COMMON_HEADERS}

			"include/lotus/graphics/backends/directx12/commands.h"
			"include/lotus/graphics/backends/directx12/context.h"
			"include/lotus/graphics/backends/directx12/descriptors.h"
			"include/lotus/graphics/backends/directx12/details.h"
			"include/lotus/graphics/backends/directx12/device.h"
			"include/lotus/graphics/backends/directx12/frame_buffer.h"
			"include/lotus/graphics/backends/directx12/pass.h"
			"include/lotus/graphics/backends/directx12/pipeline.h"
			"include/lotus/graphics/backends/directx12/resources.h"
			"include/lotus/graphics/backends/directx12/synchronization.h"

			"include/lotus/graphics/backends/directx12.h"
		PRIVATE
			"src/graphics/backends/directx12/commands.cpp"
			"src/graphics/backends/directx12/context.cpp"
			"src/graphics/backends/directx12/descriptors.cpp"
			"src/graphics/backends/directx12/details.cpp"
			"src/graphics/backends/directx12/device.cpp"
			"src/graphics/backends/directx12/frame_buffer.cpp"

			"src/graphics/common.cpp")
	target_link_libraries(lotus_graphics_directx12
		PUBLIC
			lotus_core lotus_system d3d12 dxgi dxguid d3dcompiler dxcompiler Microsoft::DirectX-Headers)
	target_compile_definitions(lotus_graphics_directx12
		PUBLIC
			"${LOTUS_GRAPHICS_BACKEND_INCLUDE_NAME}=<lotus/graphics/backends/directx12.h>")
endif()

# Vulkan
if(LOTUS_GRAPHICS_USE_VULKAN)
	find_package(Vulkan REQUIRED)
	# SPIRV-Reflect
	add_library(SPIRV_Reflect STATIC)
	target_sources(SPIRV_Reflect PRIVATE "${Vulkan_INCLUDE_DIR}/../Source/SPIRV-Reflect/spirv_reflect.c")
	target_include_directories(SPIRV_Reflect INTERFACE "${Vulkan_INCLUDE_DIR}/../Source/SPIRV-Reflect/")
	target_link_libraries(SPIRV_Reflect PUBLIC Vulkan::Vulkan)

	add_library(lotus_graphics_vulkan STATIC)
	set_target_compile_options(lotus_graphics_vulkan)

	target_include_directories(lotus_graphics_vulkan PUBLIC "include/")
	target_sources(lotus_graphics_vulkan
		PUBLIC
			${LOTUS_GRAPHICS_COMMON_HEADERS}

			"include/lotus/graphics/backends/vulkan/commands.h"
			"include/lotus/graphics/backends/vulkan/context.h"
			"include/lotus/graphics/backends/vulkan/descriptors.h"
			"include/lotus/graphics/backends/vulkan/details.h"
			"include/lotus/graphics/backends/vulkan/device.h"
			"include/lotus/graphics/backends/vulkan/frame_buffer.h"
			"include/lotus/graphics/backends/vulkan/pass.h"
			"include/lotus/graphics/backends/vulkan/pipeline.h"
			"include/lotus/graphics/backends/vulkan/resources.h"
			"include/lotus/graphics/backends/vulkan/synchronization.h"

			"include/lotus/graphics/backends/vulkan.h"
		PRIVATE
			"src/graphics/backends/vulkan/commands.cpp"
			"src/graphics/backends/vulkan/context.cpp"
			"src/graphics/backends/vulkan/details.cpp"
			"src/graphics/backends/vulkan/device.cpp"
			"src/graphics/backends/vulkan/frame_buffer.cpp"
			
			"src/graphics/common.cpp")
	target_link_libraries(lotus_graphics_vulkan
		PUBLIC
			lotus_core lotus_system Vulkan::Vulkan SPIRV_Reflect)
	target_compile_definitions(lotus_graphics_vulkan
		PUBLIC
			VULKAN_HPP_NO_EXCEPTIONS
			VK_USE_PLATFORM_WIN32_KHR
			"${LOTUS_GRAPHICS_BACKEND_INCLUDE_NAME}=<lotus/graphics/backends/vulkan.h>")
endif()
