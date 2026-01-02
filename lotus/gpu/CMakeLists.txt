option(LOTUS_GPU_USE_DIRECTX12 "Whether to generate target for the DirectX 12 backend." YES)
option(LOTUS_GPU_USE_VULKAN "Whether to generate target for the Vulkan backend." YES)
option(LOTUS_GPU_USE_METAL "Whether to generate target for the Metal backend." YES)

option(LOTUS_GPU_DIRECTX12_USE_PIX "Whether to use the PIX API if it's available." YES)

option(LOTUS_GPU_VULKAN_USE_SYSTEM_DXC "Whether to use system DXC for vulkan instead of the one bundled in Vulkan SDK." NO)

set(LOTUS_GPU_COMMON_HEADERS
	"include/lotus/gpu/backends/common/details.h"
	"include/lotus/gpu/backends/common/dxc.h"
	"include/lotus/gpu/backends/common/dxgi_format.h"
	"include/lotus/gpu/backends/common/dxil_reflection.h"

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
	"src/gpu/backends/common/details.cpp"
	"src/gpu/backends/common/dxc.cpp"
	"src/gpu/backends/common/dxgi_format.cpp"
	"src/gpu/backends/common/dxil_reflection.cpp"

	"src/gpu/common.cpp")

add_subdirectory("thirdparty/DirectX-Headers/")

function(add_gpu_backend_include_definitions TARGET INCLUDE_DIR)
	target_compile_definitions(${TARGET}
		PUBLIC
			"LOTUS_GPU_BACKEND_INCLUDE_ACCELERATION_STRUCTURE=<${INCLUDE_DIR}acceleration_structure.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_COMMANDS=<${INCLUDE_DIR}commands.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_COMMON=<${INCLUDE_DIR}common.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_CONTEXT=<${INCLUDE_DIR}context.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_DESCRIPTORS=<${INCLUDE_DIR}descriptors.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_DEVICE=<${INCLUDE_DIR}device.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_FRAME_BUFFER=<${INCLUDE_DIR}frame_buffer.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_PIPELINE=<${INCLUDE_DIR}pipeline.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_RESOURCES=<${INCLUDE_DIR}resources.h>"
			"LOTUS_GPU_BACKEND_INCLUDE_SYNCHRONIZATION=<${INCLUDE_DIR}synchronization.h>")
endfunction()

# DirectX 12
set(LOTUS_GPU_HAS_DIRECTX12 NO CACHE INTERNAL "Whether the DirectX12 backend is available.")
if(WIN32 AND LOTUS_GPU_USE_DIRECTX12)
	set(LOTUS_GPU_HAS_DIRECTX12 YES CACHE INTERNAL "Whether the DirectX12 backend is available.")
	list(APPEND ALL_BACKENDS directx12)

	add_library(lotus_gpu_directx12 STATIC)
	configure_lotus_module(lotus_gpu_directx12)

	target_include_directories(lotus_gpu_directx12 PUBLIC "include/")
	target_sources(lotus_gpu_directx12
		PUBLIC
			${LOTUS_GPU_COMMON_HEADERS}

			"include/lotus/gpu/backends/directx12/acceleration_structure.h"
			"include/lotus/gpu/backends/directx12/commands.h"
			"include/lotus/gpu/backends/directx12/common.h"
			"include/lotus/gpu/backends/directx12/context.h"
			"include/lotus/gpu/backends/directx12/descriptors.h"
			"include/lotus/gpu/backends/directx12/details.h"
			"include/lotus/gpu/backends/directx12/device.h"
			"include/lotus/gpu/backends/directx12/frame_buffer.h"
			"include/lotus/gpu/backends/directx12/pipeline.h"
			"include/lotus/gpu/backends/directx12/resources.h"
			"include/lotus/gpu/backends/directx12/synchronization.h"
		PRIVATE
			${LOTUS_GPU_COMMON_SOURCES}

			"src/gpu/backends/directx12/commands.cpp"
			"src/gpu/backends/directx12/context.cpp"
			"src/gpu/backends/directx12/descriptors.cpp"
			"src/gpu/backends/directx12/details.cpp"
			"src/gpu/backends/directx12/device.cpp"
			"src/gpu/backends/directx12/frame_buffer.cpp")
	add_gpu_backend_include_definitions(lotus_gpu_directx12 "lotus/gpu/backends/directx12/")
	target_link_libraries(lotus_gpu_directx12
		PUBLIC
			lotus_core lotus_system d3d12 dxgi dxguid d3dcompiler dxcompiler Microsoft::DirectX-Headers)
	target_compile_definitions(lotus_gpu_directx12
		PUBLIC
			"LOTUS_GPU_DXC_HEADER=<dxcapi.h>")

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
	list(APPEND ALL_BACKENDS vulkan)

	if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND LOTUS_GPU_VULKAN_USE_SYSTEM_DXC)
		set(USE_SYSTEM_DXC YES)
	endif()

	if(NOT USE_SYSTEM_DXC)
		list(APPEND LOTUS_GPU_VULKAN_VULKAN_COMPONENTS dxc)
	endif()

	find_package(Vulkan REQUIRED COMPONENTS ${LOTUS_GPU_VULKAN_VULKAN_COMPONENTS})
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
			"include/lotus/gpu/backends/vulkan/common.h"
			"include/lotus/gpu/backends/vulkan/context.h"
			"include/lotus/gpu/backends/vulkan/descriptors.h"
			"include/lotus/gpu/backends/vulkan/details.h"
			"include/lotus/gpu/backends/vulkan/device.h"
			"include/lotus/gpu/backends/vulkan/frame_buffer.h"
			"include/lotus/gpu/backends/vulkan/pipeline.h"
			"include/lotus/gpu/backends/vulkan/resources.h"
			"include/lotus/gpu/backends/vulkan/synchronization.h"
		PRIVATE
			${LOTUS_GPU_COMMON_SOURCES}

			"src/gpu/backends/vulkan/commands.cpp"
			"src/gpu/backends/vulkan/context.cpp"
			"src/gpu/backends/vulkan/details.cpp"
			"src/gpu/backends/vulkan/device.cpp"
			"src/gpu/backends/vulkan/pipeline.cpp"
			"src/gpu/backends/vulkan/resources.cpp")
	add_gpu_backend_include_definitions(lotus_gpu_vulkan "lotus/gpu/backends/vulkan/")
	target_link_libraries(lotus_gpu_vulkan
		PUBLIC
			lotus_core lotus_system Vulkan::Vulkan SPIRV_Reflect Microsoft::DirectX-Headers)
	target_compile_definitions(lotus_gpu_vulkan
		PUBLIC
			VULKAN_HPP_NO_EXCEPTIONS)

	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		# Windows-specific settings
		target_compile_definitions(lotus_gpu_vulkan PUBLIC VK_USE_PLATFORM_WIN32_KHR)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		# MoltenVK-specific settings
		enable_language(OBJCXX)
		target_sources(lotus_gpu_vulkan
			PUBLIC
				"include/lotus/gpu/backends/vulkan/moltenvk.h"
			PRIVATE
				"src/gpu/backends/vulkan/moltenvk.mm")
		find_library(QUARTZ_CORE_LIB QuartzCore)
		find_library(METAL_LIB Metal)
		target_link_libraries(lotus_gpu_vulkan
			PUBLIC
				"${QUARTZ_CORE_LIB}" "${METAL_LIB}")
		target_compile_options(lotus_gpu_vulkan
			PUBLIC
				-fms-extensions)
		target_compile_definitions(lotus_gpu_vulkan PUBLIC VK_USE_PLATFORM_METAL_EXT)
	endif()

	# if using DXC for shader compilation, find and link DXC
	if(USE_SYSTEM_DXC)
		target_link_libraries(lotus_gpu_vulkan PUBLIC dxcompiler)
		target_compile_definitions(lotus_gpu_vulkan
			PUBLIC
				"LOTUS_GPU_DXC_HEADER=<dxcapi.h>")
	else()
		target_link_libraries(lotus_gpu_vulkan PUBLIC Vulkan::dxc_lib)
		# unlike the Windows SDK, the Vulkan SDK puts dxcapi.h under a folder
		target_compile_definitions(lotus_gpu_vulkan
			PUBLIC
				"LOTUS_GPU_DXC_HEADER=<dxc/dxcapi.h>")
	endif()
endif()

# Metal
set(LOTUS_GPU_HAS_METAL NO CACHE INTERNAL "Whether the Metal backend is available.")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND LOTUS_GPU_USE_METAL)
	set(LOTUS_GPU_HAS_METAL YES CACHE INTERNAL "Whether the Metal backend is available.")
	list(APPEND ALL_BACKENDS metal)

	enable_language(OBJCXX)
	add_library(lotus_gpu_metal STATIC)
	configure_lotus_module(lotus_gpu_metal)

	target_include_directories(lotus_gpu_metal PUBLIC "include/")
	target_sources(lotus_gpu_metal
		PUBLIC
			${LOTUS_GPU_COMMON_HEADERS}

			"include/lotus/gpu/backends/metal/acceleration_structure.h"
			"include/lotus/gpu/backends/metal/commands.h"
			"include/lotus/gpu/backends/metal/common.h"
			"include/lotus/gpu/backends/metal/context.h"
			"include/lotus/gpu/backends/metal/descriptors.h"
			"include/lotus/gpu/backends/metal/details.h"
			"include/lotus/gpu/backends/metal/device.h"
			"include/lotus/gpu/backends/metal/frame_buffer.h"
			"include/lotus/gpu/backends/metal/pipeline.h"
			"include/lotus/gpu/backends/metal/resources.h"
			"include/lotus/gpu/backends/metal/synchronization.h"
		PRIVATE
			${LOTUS_GPU_COMMON_SOURCES}

			"src/gpu/backends/metal/commands.cpp"
			"src/gpu/backends/metal/context.cpp"
			"src/gpu/backends/metal/context.mm"
			"src/gpu/backends/metal/details.cpp"
			"src/gpu/backends/metal/device.cpp"
			"src/gpu/backends/metal/device.mm"
			"src/gpu/backends/metal/metal_impl.cpp")
	add_gpu_backend_include_definitions(lotus_gpu_metal "lotus/gpu/backends/metal/")
	target_include_directories(lotus_gpu_metal PUBLIC "thirdparty/metal-cpp/")
	find_library(QUARTZ_CORE_LIB QuartzCore)
	find_library(METAL_LIB Metal)
	find_library(METAL_SHADER_CONVERTER_LIB NAMES libmetalirconverter.dylib)
	target_link_libraries(lotus_gpu_metal
		PUBLIC
			lotus_core
			lotus_system
			"${QUARTZ_CORE_LIB}"
			"${METAL_LIB}"
			Microsoft::DirectX-Headers
			"${METAL_SHADER_CONVERTER_LIB}")
	target_compile_options(lotus_gpu_metal
		PUBLIC
			-fms-extensions)

	# TODO temporarily using Vulkan DXC for Metal. should we compile DXC ourselves?
	find_package(Vulkan REQUIRED COMPONENTS dxc)
	target_link_libraries(lotus_gpu_metal
		PUBLIC
			Vulkan::Vulkan Vulkan::dxc_lib)  # for some reason need to also link Vulkan
	target_compile_definitions(lotus_gpu_metal
		PUBLIC
			"LOTUS_GPU_DXC_HEADER=<dxc/dxcapi.h>")
endif()


set(LOTUS_GPU_ALL_AVAILABLE_BACKENDS ${ALL_BACKENDS} CACHE INTERNAL "List of all available GPU backends.")


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
				${DXC_EXTRA_ARGS}
		COMMENT "Compiling shader ${FILENAME}"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
	add_dependencies("${DEPENDENT}" "${TARGET_NAME}")
endfunction()
