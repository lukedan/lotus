find_package(imgui CONFIG REQUIRED)

function(add_testbed_executable GPU_BACKEND)
	set(PROJ testbed_${GPU_BACKEND})

	add_executable(${PROJ})
	target_sources(${PROJ}
		PRIVATE
			"tests/box_stack_test.h"
			"tests/fem_cloth_test.h"
			"tests/polyhedron_test.h"
			"tests/spring_cloth_test.h"
			"tests/test.h"

			"testbed.cpp"
			"utils.cpp"
			"utils.h")
	target_link_libraries(${PROJ} PRIVATE lotus_system lotus_renderer_${GPU_BACKEND} lotus_physics imgui::imgui)
	target_include_directories(${PROJ} PRIVATE "../common/include")
	set_target_properties(${PROJ} PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION ON)
endfunction()

add_testbed_executable(vulkan)
add_testbed_executable(directx12)
