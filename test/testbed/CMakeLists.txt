add_executable(testbed)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)

target_sources(testbed
	PRIVATE
		"tests/fem_cloth_test.h"
		"tests/spring_cloth_test.h"

		"test.h"
		"testbed.cpp"
		"utils.cpp"
		"utils.h")
target_link_libraries(testbed PRIVATE pbd glfw OpenGL::GL imgui::imgui)
