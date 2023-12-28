option(LOTUS_GPU_USE_DIRECTX12 "Whether to generate target for the DirectX 12 backend." YES)
option(LOTUS_GPU_USE_VULKAN "Whether to generate target for the Vulkan backend." YES)

option(LOTUS_GPU_DIRECTX12_USE_PIX "Whether to use the PIX API if it's available." YES)

option(LOTUS_GPU_VULKAN_USE_SYSTEM_DXC "Whether to use system DXC for vulkan instead of the one bundled in Vulkan SDK." NO)

set(LOTUS_GPU_COMMON_HEADERS
	"include/lotus/gpu/backends/common/dxc.h"
	"include/lotus/gpu/backends/common/dxgi_format.h"

	"include/lotus/gpu/acceleration_structure.h"
	"include/lotus/gpu/commands.h"
	"include/lotus/gpu/common.h"
	"include/lotus/gpu/context.h"
	"include/lotus/gpu/descriptors.h"
	"include/lotus/gpu/device.h"
	"include/lotus/gpu/frame_buffer.h"
	"include/lotus/gpu/pipeline.h"
	"include/lotus/gpu/resources.h"
	"include/lotus/gpu/synchronization.h")
set(LOTUS_GPU_COMMON_SOURCES
	"src/gpu/backends/common/dxc.cpp"
	"src/gpu/backends/common/dxgi_format.cpp")

add_subdirectory("thirdparty/DirectX-Headers/")

# DirectX 12
set(LOTUS_GPU_HAS_DIRECTX12 NO CACHE INTERNAL "Whether the DirectX12 backend is available.")
if(WIN32 AND LOTUS_GPU_USE_DIRECTX12)
	set(LOTUS_GPU_HAS_DIRECTX12 YES CACHE INTERNAL "Whether the DirectX12 backend is available.")

	add_library(lotus_gpu_directx12 STATIC)
	configure_lotus_module(lotus_gpu_directx12)

	target_include_directories(lotus_gpu_directx12 PUBLIC "include/")
	target_sources(lotus_gpu_directx12
		PUBLIC
			${LOTUS_GPU_COMMON_HEADERS}

			"include/lotus/gpu/backends/directx12/acceleration_structure.h"
			"include/lotus/gpu/backends/directx12/commands.h"
			"include/lotus/gpu/backends/directx12/context.h"
			"include/lotus/gpu/backends/directx12/descriptors.h"
			"include/lotus/gpu/backends/directx12/details.h"
			"include/lotus/gpu/backends/directx12/device.h"
			"include/lotus/gpu/backends/directx12/frame_buffer.h"
			"include/lotus/gpu/backends/directx12/pipeline.h"
			"include/lotus/gpu/backends/directx12/resources.h"
			"include/lotus/gpu/backends/directx12/synchronization.h"

			"include/lotus/gpu/backends/directx12.h"
		PRIVATE
			${LOTUS_GPU_COMMON_SOURCES}

			"src/gpu/backends/directx12/commands.cpp"
			"src/gpu/backends/directx12/context.cpp"
			"src/gpu/backends/directx12/descriptors.cpp"
			"src/gpu/backends/directx12/details.cpp"
			"src/gpu/backends/directx12/device.cpp"
			"src/gpu/backends/directx12/frame_buffer.cpp"
			"src/gpu/backends/directx12/pipeline.cpp"

			"src/gpu/common.cpp")
	target_link_libraries(lotus_gpu_directx12
		PUBLIC
			lotus_core lotus_system d3d12 dxgi dxguid d3dcompiler dxcompiler Microsoft::DirectX-Headers)
	target_compile_definitions(lotus_gpu_directx12
		PUBLIC
			"LOTUS_GPU_BACKEND_INCLUDE=<lotus/gpu/backends/directx12.h>")

	# WinPixEventRuntime support
	if(LOTUS_GPU_DIRECTX12_USE_PIX)
		set(PIX_EVENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/WinPixEventRuntime/")
		if(EXISTS "${PIX_EVENT_DIR}")
			add_library(lotus_gpu_directx12_pix_event SHARED IMPORTED)
			target_include_directories(lotus_gpu_directx12_pix_event
				INTERFACE "${PIX_EVENT_DIR}/Include/")
			target_compile_definitions(lotus_gpu_directx12_pix_event INTERFACE USE_PIX)
			set_target_properties(lotus_gpu_directx12_pix_event
				PROPERTIES
					IMPORTED_IMPLIB "${PIX_EVENT_DIR}/bin/x64/WinPixEventRuntime.lib"
					IMPORTED_LOCATION "${PIX_EVENT_DIR}/bin/x64/WinPixEventRuntime.dll")

			target_link_libraries(lotus_gpu_directx12 PRIVATE lotus_gpu_directx12_pix_event)
		else()
			message(WARNING
				"Pix event runtime not found. Please follow the instructions on "
				"https://devblogs.microsoft.com/pix/winpixeventruntime/, and place the extracted files in "
				"lotus/gpu/thirdparty/WinPixEventRuntime.")
		endif()
	endif()
endif()

# Vulkan
set(LOTUS_GPU_HAS_VULKAN NO CACHE INTERNAL "Whether the Vulkan backend is available.")
if(LOTUS_GPU_USE_VULKAN)
	set(LOTUS_GPU_HAS_VULKAN YES CACHE INTERNAL "Whether the Vulkan backend is available.")

	find_package(Vulkan REQUIRED)
	# SPIRV-Reflect
	add_library(SPIRV_Reflect STATIC)
	target_sources(SPIRV_Reflect PRIVATE "thirdparty/SPIRV-Reflect/spirv_reflect.c")
	target_include_directories(SPIRV_Reflect INTERFACE "thirdparty/SPIRV-Reflect/")
	target_link_libraries(SPIRV_Reflect PUBLIC Vulkan::Vulkan)

	add_library(lotus_gpu_vulkan STATIC)
	configure_lotus_module(lotus_gpu_vulkan)

	target_include_directories(lotus_gpu_vulkan PUBLIC "include/")
	target_sources(lotus_gpu_vulkan
		PUBLIC
			${LOTUS_GPU_COMMON_HEADERS}

			"include/lotus/gpu/backends/vulkan/acceleration_structure.h"
			"include/lotus/gpu/backends/vulkan/commands.h"
			"include/lotus/gpu/backends/vulkan/context.h"
			"include/lotus/gpu/backends/vulkan/descriptors.h"
			"include/lotus/gpu/backends/vulkan/details.h"
			"include/lotus/gpu/backends/vulkan/device.h"
			"include/lotus/gpu/backends/vulkan/frame_buffer.h"
			"include/lotus/gpu/backends/vulkan/pipeline.h"
			"include/lotus/gpu/backends/vulkan/resources.h"
			"include/lotus/gpu/backends/vulkan/synchronization.h"

			"include/lotus/gpu/backends/vulkan.h"
		PRIVATE
			${LOTUS_GPU_COMMON_SOURCES}

			"src/gpu/backends/vulkan/commands.cpp"
			"src/gpu/backends/vulkan/context.cpp"
			"src/gpu/backends/vulkan/details.cpp"
			"src/gpu/backends/vulkan/device.cpp"
			"src/gpu/backends/vulkan/frame_buffer.cpp"
			"src/gpu/backends/vulkan/pipeline.cpp"
			"src/gpu/backends/vulkan/resources.cpp"
			
			"src/gpu/common.cpp")
	target_link_libraries(lotus_gpu_vulkan
		PUBLIC
			lotus_core lotus_system Vulkan::Vulkan SPIRV_Reflect Microsoft::DirectX-Headers)
	target_compile_definitions(lotus_gpu_vulkan
		PUBLIC
			VULKAN_HPP_NO_EXCEPTIONS
			"LOTUS_GPU_BACKEND_INCLUDE=<lotus/gpu/backends/vulkan.h>")
	if(WIN32)
		target_compile_definitions(lotus_gpu_vulkan PUBLIC VK_USE_PLATFORM_WIN32_KHR)
	endif()

	# if using DXC for shader compilation, find and link DXC
	if(LOTUS_GPU_VULKAN_USE_SYSTEM_DXC)
		target_link_libraries(lotus_gpu_vulkan PUBLIC dxcompiler)
	else()
		set(VULKAN_SDK_PATH "${Vulkan_LIBRARY}/../../")
		find_library(LOTUS_GPU_VULKAN_DXC_LIBRARY
			dxcompiler
			PATHS "${VULKAN_SDK_PATH}/Lib/"
			REQUIRED
			NO_DEFAULT_PATH)
		target_link_libraries(lotus_gpu_vulkan PUBLIC "${LOTUS_GPU_VULKAN_DXC_LIBRARY}")
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
