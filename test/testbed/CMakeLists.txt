add_executable(testbed)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)

target_sources(testbed
	PRIVATE
		"tests/box_stack_test.h"
		"tests/fem_cloth_test.h"
		"tests/polyhedron_test.h"
		"tests/spring_cloth_test.h"
		"tests/test.h"

		"testbed.cpp"
		"utils.cpp"
		"utils.h")
target_link_libraries(testbed PRIVATE lotus_physics glfw OpenGL::GL imgui::imgui)
set_target_properties(testbed PROPERTIES
	INTERPROCEDURAL_OPTIMIZATION ON)
