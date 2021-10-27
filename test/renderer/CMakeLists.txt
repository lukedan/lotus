add_executable(renderer)

function(add_shader FILENAME STAGE ENTRY PROFILE DEPENDENT BACKEND)
	# output file name
	cmake_path(REPLACE_EXTENSION FILENAME LAST_ONLY ".${STAGE}.o" OUTPUT_VARIABLE OUTPUT_FILENAME)
	cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR "shaders" "${OUTPUT_FILENAME}" OUTPUT_VARIABLE OUTPUT_FILENAME)
	# make directory
	cmake_path(GET OUTPUT_FILENAME PARENT_PATH OUTPUT_DIRECTORY)
	file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
	# target name
	string(REGEX REPLACE "/|\\\\|:" "_" TARGET_NAME "${OUTPUT_FILENAME}")
	string(PREPEND TARGET_NAME "shader")
	# command and dependency
	if(BACKEND STREQUAL "directx12")
		set(DXC_EXTRA_ARGS)
		set(DXC_EXECUTABLE dxc)
	elseif(BACKEND STREQUAL "vulkan")
		set(DXC_EXTRA_ARGS -spirv -fspv-reflect)
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
				-Fo "${OUTPUT_FILENAME}"
				-E "${ENTRY}"
				-Ges
				-Zpr
				-Zi
				${DXC_EXTRA_ARGS}
		COMMENT "Compiling shader ${FILENAME}"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
	add_dependencies("${DEPENDENT}" "${TARGET_NAME}")
endfunction()

set(LOTUS_RENDERER_BACKEND vulkan)

target_sources(renderer
	PRIVATE
		"common.h"
		"composite_pass.h"
		"gbuffer_pass.h"
		"scene.h"

		"main.cpp")
add_shader("gbuffer.hlsl" vs main_vs 6_0 renderer ${LOTUS_RENDERER_BACKEND})
add_shader("gbuffer.hlsl" ps main_ps 6_0 renderer ${LOTUS_RENDERER_BACKEND})
add_shader("composite.hlsl" vs main_vs 6_0 renderer ${LOTUS_RENDERER_BACKEND})
add_shader("composite.hlsl" ps main_ps 6_0 renderer ${LOTUS_RENDERER_BACKEND})
target_link_libraries(renderer PRIVATE lotus_graphics_${LOTUS_RENDERER_BACKEND} tinygltf)
set_target_properties(renderer PROPERTIES
	INTERPROCEDURAL_OPTIMIZATION ON)
