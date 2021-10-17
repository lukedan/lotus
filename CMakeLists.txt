cmake_minimum_required(VERSION 3.15)
project(lotus)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC


set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_HEADER_ONLY OFF CACHE INTERNAL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/thirdparty/tinygltf")


# DirectXShaderCompiler
set(LOTUS_DXC_TARGET_ARCH "x64" CACHE STRING "Architecture used to build DXC.")

# DirectXShaderCompiler requires additional arguments in the file
file(READ "thirdparty/DirectXShaderCompiler/utils/cmake-predefined-config-params" LOTUS_SHADER_PARSER_LLVM_OPTIONS)
separate_arguments(LOTUS_SHADER_PARSER_LLVM_OPTIONS UNIX_COMMAND ${LOTUS_SHADER_PARSER_LLVM_OPTIONS})
foreach(LOTUS_ARG ${LOTUS_SHADER_PARSER_LLVM_OPTIONS})
	if("${LOTUS_ARG}" MATCHES "-D([^:]+):([^=]+)=(.+)")
		message("LLVM argument: ${CMAKE_MATCH_1}, type: ${CMAKE_MATCH_2}, value: ${CMAKE_MATCH_3}")
		set(${CMAKE_MATCH_1} "${CMAKE_MATCH_3}" CACHE "${CMAKE_MATCH_2}" "Additional argument for LLVM used by DXC")
	elseif("${LOTUS_ARG}" MATCHES "-D([^:]+)=(.+)")
		message("LLVM argument: ${CMAKE_MATCH_1}, value: ${CMAKE_MATCH_2}")
		set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE)
	else()
		message(FATAL_ERROR "Failed to parse argument: ${LOTUS_ARG}")
	endif()
endforeach()
set(DXC_BUILD_ARCH "${LOTUS_DXC_TARGET_ARCH}")

# We must use the Visual Studio generator instead of the Ninja one so that DIA SDK can be successfully found
add_subdirectory("thirdparty/DirectXShaderCompiler/")


include("cmake/include.cmake")

add_subdirectory("lotus/core/")
add_subdirectory("lotus/physics/")
add_subdirectory("system/")
add_subdirectory("lotus/system/")
add_subdirectory("lotus/graphics/")

add_subdirectory("test/")
