cmake_minimum_required(VERSION 3.15)
project(lotus)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC


set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_HEADER_ONLY OFF CACHE INTERNAL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/thirdparty/tinygltf")


include("cmake/include.cmake")

add_subdirectory("lotus/core/")
add_subdirectory("lotus/physics/")
add_subdirectory("system/")
add_subdirectory("lotus/system/")
add_subdirectory("lotus/graphics/")

add_subdirectory("test/")
