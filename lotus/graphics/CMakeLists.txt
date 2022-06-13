set(LOTUS_GRAPHICS_USE_DIRECTX12 YES CACHE STRING "Whether to generate target for the DirectX 12 backend.")
set(LOTUS_GRAPHICS_USE_VULKAN YES CACHE STRING "Whether to generate target for the Vulkan backend.")

set(LOTUS_GRAPHICS_VULKAN_USE_SYSTEM_DXC NO CACHE STRING "Whether to use system DXC for vulkan instead of the one bundled in Vulkan SDK.")

set(LOTUS_GRAPHICS_COMMON_HEADERS
	"include/lotus/graphics/backends/common/dxc.h"

	"include/lotus/graphics/acceleration_structure.h"
	"include/lotus/graphics/commands.h"
	"include/lotus/graphics/common.h"
	"include/lotus/graphics/context.h"
	"include/lotus/graphics/descriptors.h"
	"include/lotus/graphics/device.h"
	"include/lotus/graphics/frame_buffer.h"
	"include/lotus/graphics/pipeline.h"
	"include/lotus/graphics/resources.h"
	"include/lotus/graphics/synchronization.h")
set(LOTUS_GRAPHICS_COMMON_SOURCES
	"src/graphics/backends/common/dxc.cpp")

# DirectX 12
set(LOTUS_GRAPHICS_HAS_DIRECTX12 NO CACHE INTERNAL "Whether the DirectX12 backend is available." FORCE)
if(WIN32 AND LOTUS_GRAPHICS_USE_DIRECTX12)
	set(LOTUS_GRAPHICS_HAS_DIRECTX12 YES CACHE INTERNAL "Whether the DirectX12 backend is available." FORCE)

	add_library(lotus_graphics_directx12 STATIC)
	configure_lotus_module(lotus_graphics_directx12)

	add_subdirectory("thirdparty/DirectX-Headers/")

	target_include_directories(lotus_graphics_directx12 PUBLIC "include/")
	target_sources(lotus_graphics_directx12
		PUBLIC
			${LOTUS_GRAPHICS_COMMON_HEADERS}

			"include/lotus/graphics/backends/directx12/acceleration_structure.h"
			"include/lotus/graphics/backends/directx12/commands.h"
			"include/lotus/graphics/backends/directx12/context.h"
			"include/lotus/graphics/backends/directx12/descriptors.h"
			"include/lotus/graphics/backends/directx12/details.h"
			"include/lotus/graphics/backends/directx12/device.h"
			"include/lotus/graphics/backends/directx12/frame_buffer.h"
			"include/lotus/graphics/backends/directx12/pipeline.h"
			"include/lotus/graphics/backends/directx12/resources.h"
			"include/lotus/graphics/backends/directx12/synchronization.h"

			"include/lotus/graphics/backends/directx12.h"
		PRIVATE
			${LOTUS_GRAPHICS_COMMON_SOURCES}

			"src/graphics/backends/directx12/commands.cpp"
			"src/graphics/backends/directx12/context.cpp"
			"src/graphics/backends/directx12/descriptors.cpp"
			"src/graphics/backends/directx12/details.cpp"
			"src/graphics/backends/directx12/device.cpp"
			"src/graphics/backends/directx12/frame_buffer.cpp"
			"src/graphics/backends/directx12/pipeline.cpp"

			"src/graphics/common.cpp")
	target_link_libraries(lotus_graphics_directx12
		PUBLIC
			lotus_core lotus_system d3d12 dxgi dxguid d3dcompiler dxcompiler Microsoft::DirectX-Headers)
	target_compile_definitions(lotus_graphics_directx12
		PUBLIC
			"LOTUS_GRAPHICS_BACKEND_INCLUDE=<lotus/graphics/backends/directx12.h>")
endif()

# Vulkan
set(LOTUS_GRAPHICS_HAS_VULKAN YES CACHE INTERNAL "Whether the Vulkan backend is available." FORCE)
if(LOTUS_GRAPHICS_USE_VULKAN)
	set(LOTUS_GRAPHICS_HAS_VULKAN YES CACHE INTERNAL "Whether the Vulkan backend is available." FORCE)

	find_package(Vulkan REQUIRED)
	# SPIRV-Reflect
	add_library(SPIRV_Reflect STATIC)
	target_sources(SPIRV_Reflect PRIVATE "thirdparty/SPIRV-Reflect/spirv_reflect.c")
	target_include_directories(SPIRV_Reflect INTERFACE "thirdparty/SPIRV-Reflect/")
	target_link_libraries(SPIRV_Reflect PUBLIC Vulkan::Vulkan)

	add_library(lotus_graphics_vulkan STATIC)
	configure_lotus_module(lotus_graphics_vulkan)

	target_include_directories(lotus_graphics_vulkan PUBLIC "include/")
	target_sources(lotus_graphics_vulkan
		PUBLIC
			${LOTUS_GRAPHICS_COMMON_HEADERS}

			"include/lotus/graphics/backends/vulkan/acceleration_structure.h"
			"include/lotus/graphics/backends/vulkan/commands.h"
			"include/lotus/graphics/backends/vulkan/context.h"
			"include/lotus/graphics/backends/vulkan/descriptors.h"
			"include/lotus/graphics/backends/vulkan/details.h"
			"include/lotus/graphics/backends/vulkan/device.h"
			"include/lotus/graphics/backends/vulkan/frame_buffer.h"
			"include/lotus/graphics/backends/vulkan/pipeline.h"
			"include/lotus/graphics/backends/vulkan/resources.h"
			"include/lotus/graphics/backends/vulkan/synchronization.h"

			"include/lotus/graphics/backends/vulkan.h"
		PRIVATE
			${LOTUS_GRAPHICS_COMMON_SOURCES}

			"src/graphics/backends/vulkan/commands.cpp"
			"src/graphics/backends/vulkan/context.cpp"
			"src/graphics/backends/vulkan/details.cpp"
			"src/graphics/backends/vulkan/device.cpp"
			"src/graphics/backends/vulkan/frame_buffer.cpp"
			"src/graphics/backends/vulkan/pipeline.cpp"
			
			"src/graphics/common.cpp")
	target_link_libraries(lotus_graphics_vulkan
		PUBLIC
			lotus_core lotus_system Vulkan::Vulkan SPIRV_Reflect)
	target_compile_definitions(lotus_graphics_vulkan
		PUBLIC
			VULKAN_HPP_NO_EXCEPTIONS
			"LOTUS_GRAPHICS_BACKEND_INCLUDE=<lotus/graphics/backends/vulkan.h>")
	if(WIN32)
		target_compile_definitions(lotus_graphics_vulkan PUBLIC VK_USE_PLATFORM_WIN32_KHR)
	endif()

	# if using DXC for shader compilation, find and link DXC
	if(LOTUS_GRAPHICS_VULKAN_USE_SYSTEM_DXC)
		target_link_libraries(lotus_graphics_vulkan PUBLIC dxcompiler)
	else()
		cmake_path(GET Vulkan_LIBRARY PARENT_PATH LOTUS_VULKAN_LIBRARY_PATH)
		find_file(
			LOTUS_VULKAN_DXC_LIBRARY
			NAMES dxcompiler.lib libdxcompiler.a libdxcompiler.so
			PATHS "${LOTUS_VULKAN_LIBRARY_PATH}"
			NO_DEFAULT_PATH)
		target_link_libraries(lotus_graphics_vulkan PUBLIC "${LOTUS_VULKAN_DXC_LIBRARY}")
	endif()
endif()


function(add_shader FILENAME STAGE ENTRY PROFILE DEPENDENT BACKEND)
	# output file name
	cmake_path(REPLACE_EXTENSION FILENAME LAST_ONLY ".${STAGE}.o" OUTPUT_VARIABLE OUTPUT_FILENAME)
	cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR "${OUTPUT_FILENAME}" OUTPUT_VARIABLE OUTPUT_FILENAME_FULL)
	# make directory
	cmake_path(GET OUTPUT_FILENAME_FULL PARENT_PATH OUTPUT_DIRECTORY)
	file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
	# target name
	string(REGEX REPLACE "/|\\\\|:" "_" TARGET_NAME "${OUTPUT_FILENAME}")
	set(TARGET_NAME "shader_${DEPENDENT}_${TARGET_NAME}")
	# command and dependency
	if(BACKEND STREQUAL "directx12")
		set(DXC_EXTRA_ARGS)
		set(DXC_EXECUTABLE dxc)
	elseif(BACKEND STREQUAL "vulkan")
		set(DXC_EXTRA_ARGS
			-spirv
			-fspv-reflect
			-fspv-target-env=vulkan1.2
			-fvk-use-dx-layout)
		if(STAGE STREQUAL "vs" OR STAGE STREQUAL "gs" OR STAGE STREQUAL "ds")
			set(DXC_EXTRA_ARGS ${DXC_EXTRA_ARGS} -fvk-invert-y)
		endif()
		cmake_path(GET Vulkan_GLSLC_EXECUTABLE PARENT_PATH DXC_EXECUTABLE_PATH)
		find_file(DXC_EXECUTABLE
			NAMES dxc dxc.exe
			PATHS "${DXC_EXECUTABLE_PATH}"
			REQUIRED
			NO_DEFAULT_PATH)
	else()
		message(FATAL_ERROR "Unknown backend for shader compilation: ${BACKEND}")
	endif()
	add_custom_target(
		"${TARGET_NAME}"
		COMMAND
			"${DXC_EXECUTABLE}"
				"${FILENAME}"
				-T ${STAGE}_${PROFILE}
				-Fo "${OUTPUT_FILENAME_FULL}"
				-E "${ENTRY}"
				-Ges
				-Zpr
				-Zi
				-Qembed_debug
				#-no-legacy-cbuf-layout # somehow crashes the exe with access violation
				${DXC_EXTRA_ARGS}
		COMMENT "Compiling shader ${FILENAME}"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
	add_dependencies("${DEPENDENT}" "${TARGET_NAME}")
endfunction()
