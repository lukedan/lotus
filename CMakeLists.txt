cmake_minimum_required(VERSION 3.15)
project(lotus)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC

set(LOTUS_GRAPHICS_BACKEND "DirectX 12" CACHE STRING "Backend used by the library.")

set(LOTUS_SYSTEM_PLATFORM_INCLUDE_NAME "LOTUS_SYSTEM_PLATFORM_INCLUDE")
set(LOTUS_GRAPHICS_BACKEND_INCLUDE_NAME "LOTUS_GRAPHICS_BACKEND_INCLUDE")

add_library(lotus STATIC)

target_sources(lotus
	PUBLIC
		"include/lotus/algorithms/convex_hull.h"

		"include/lotus/collision/algorithms/gjk_epa.h"

		"include/lotus/collision/shapes/polyhedron.h"
		"include/lotus/collision/shapes/simple.h"

		"include/lotus/collision/shape.h"

		"include/lotus/graphics/commands.h"
		"include/lotus/graphics/common.h"
		"include/lotus/graphics/context.h"
		"include/lotus/graphics/descriptors.h"
		"include/lotus/graphics/device.h"
		"include/lotus/graphics/frame_buffer.h"
		"include/lotus/graphics/pass.h"
		"include/lotus/graphics/pipeline.h"
		"include/lotus/graphics/resources.h"
		"include/lotus/graphics/synchronization.h"

		"include/lotus/math/aab.h"
		"include/lotus/math/constants.h"
		"include/lotus/math/matrix.h"
		"include/lotus/math/quaternion.h"
		"include/lotus/math/vector.h"

		"include/lotus/physics/constraints/bend.h"
		"include/lotus/physics/constraints/contact.h"
		"include/lotus/physics/constraints/face.h"
		"include/lotus/physics/constraints/spring.h"

		"include/lotus/physics/body.h"
		"include/lotus/physics/body_properties.h"
		"include/lotus/physics/engine.h"

		"include/lotus/system/application.h"
		"include/lotus/system/common.h"
		"include/lotus/system/window.h"

		"include/lotus/utils/camera.h"
		"include/lotus/utils/stack_allocator.h"

		"include/lotus/color.h"
		"include/lotus/common.h"
		"include/lotus/memory.h"
	PRIVATE
		"src/collision/algorithms/gjk_epa.cpp"
		
		"src/collision/shapes/polyhedron.cpp"

		"src/physics/body.cpp"
		"src/physics/engine.cpp"

		"src/utils/stack_allocator.cpp")

target_include_directories(lotus PUBLIC "include/")

target_compile_features(lotus PUBLIC cxx_std_20)
set_target_properties(lotus PROPERTIES
	INTERPROCEDURAL_OPTIMIZATION ON)
# set warning level
if(MSVC)
	target_compile_options(lotus
		PUBLIC /W4 /permissive-)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(lotus
		PUBLIC -Wall -Wextra -Wconversion)
endif()

# platform support
if(WIN32)
	target_sources(lotus
		PUBLIC
			"include/lotus/system/platforms/windows/application.h"
			"include/lotus/system/platforms/windows/details.h"
			"include/lotus/system/platforms/windows/window.h"

			"include/lotus/system/platforms/windows.h"
		PRIVATE
			"src/system/platforms/windows/application.cpp"
			"src/system/platforms/windows/window.cpp")
	target_compile_definitions(lotus
		PUBLIC
			"${LOTUS_SYSTEM_PLATFORM_INCLUDE_NAME}=<lotus/system/platforms/windows.h>"
			WIN32_LEAN_AND_MEAN
			NOMINMAX)
	target_compile_definitions(lotus PUBLIC UNICODE _UNICODE)
elseif(UNIX)
endif()

# graphics backends
if(WIN32)
	if("${LOTUS_GRAPHICS_BACKEND}" STREQUAL "DirectX 12")
		target_sources(lotus
			PUBLIC
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
				"src/graphics/backends/directx12/details.cpp"
				"src/graphics/backends/directx12/device.cpp"
				"src/graphics/backends/directx12/frame_buffer.cpp")
		target_link_libraries(lotus
			PUBLIC
				d3d12 dxgi dxguid)
		target_compile_definitions(lotus
			PUBLIC
				"${LOTUS_GRAPHICS_BACKEND_INCLUDE_NAME}=<lotus/graphics/backends/directx12.h>")
	endif()
endif()

add_subdirectory("test/")
