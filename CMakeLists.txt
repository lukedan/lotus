cmake_minimum_required(VERSION 3.15)
project(lotus C CXX)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC


set(
	LOTUS_USE_ALLOCATOR "default" CACHE STRING
	"What allocator to use. Available values are \"default\" and \"mimalloc\".")


set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_HEADER_ONLY OFF CACHE INTERNAL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/thirdparty/tinygltf")


if(LOTUS_BUILD_DXC)
	# build DirectXShaderCompiler now since we'll need its binaries
	set(LOTUS_DXC_BUILD_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dxc")
	set(LOTUS_DXC_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/DirectXShaderCompiler")
	set(LOTUS_DXC_DEFAULT_GENERATOR "Ninja")
	if(WIN32)
		set(LOTUS_DXC_DEFAULT_GENERATOR "Visual Studio 16 2019")
	endif()
	set(LOTUS_DXC_GENERATOR "${LOTUS_DXC_DEFAULT_GENERATOR}" CACHE STRING "Generator used for DXC")
	message("Configuring DXC, generator: ${LOTUS_DXC_GENERATOR}, platform: ${CMAKE_GENERATOR_PLATFORM}")
	file(MAKE_DIRECTORY "${LOTUS_DXC_BUILD_DIRECTORY}")
	execute_process(
		COMMAND
			cmake
				"${LOTUS_DXC_SOURCE_DIRECTORY}"
				-G "${LOTUS_DXC_GENERATOR}"
				-A "${CMAKE_GENERATOR_PLATFORM}"
				"-DCMAKE_BUILD_TYPE=Release"
				"-DDXC_BUILD_ARCH=${CMAKE_GENERATOR_PLATFORM}"
				-C "${LOTUS_DXC_SOURCE_DIRECTORY}/cmake/caches/PredefinedParams.cmake"
		WORKING_DIRECTORY "${LOTUS_DXC_BUILD_DIRECTORY}"
		COMMAND_ERROR_IS_FATAL ANY)
	message("Building DXC")
	execute_process(
		COMMAND cmake --build "${LOTUS_DXC_BUILD_DIRECTORY}" --config Release
		COMMAND_ERROR_IS_FATAL ANY)

	# find dxc executable
	find_file(LOTUS_DXC_EXECUTABLE
		NAMES dxc.exe dxc
		PATHS "${LOTUS_DXC_BUILD_DIRECTORY}/bin/" "${LOTUS_DXC_BUILD_DIRECTORY}/Release/bin/"
		NO_DEFAULT_PATH
		REQUIRED)
	message("Built DXC executable: ${LOTUS_DXC_EXECUTABLE}")
	find_file(LOTUS_DXC_LIBRARY
		NAMES dxclib.lib libdxclib.a libdxclib.so
		PATHS "${LOTUS_DXC_BUILD_DIRECTORY}/lib/" "${LOTUS_DXC_BUILD_DIRECTORY}/Release/lib/"
		NO_DEFAULT_PATH
		REQUIRED)
	message("Built DXC library: ${LOTUS_DXC_LIBRARY}")

	# create interface target
	add_library(dxclib INTERFACE)
	target_link_libraries(dxclib INTERFACE "${LOTUS_DXC_LIBRARY}")
	target_include_directories(dxclib INTERFACE "${LOTUS_DXC_SOURCE_DIRECTORY}/include")
endif()


include("cmake/include.cmake")

add_subdirectory("lotus/core/")
add_subdirectory("lotus/physics/")
add_subdirectory("lotus/system/")
add_subdirectory("lotus/graphics/")
add_subdirectory("lotus/renderer/")

add_subdirectory("tools/")

add_subdirectory("test/")
