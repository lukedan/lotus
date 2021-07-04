add_executable(testbed)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

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
target_link_libraries(testbed PRIVATE lotus glfw OpenGL::GL)
set_target_properties(testbed PROPERTIES
	INTERPROCEDURAL_OPTIMIZATION ON)

# imgui library
add_library(imgui STATIC)
target_include_directories(imgui
	PUBLIC
		"thirdparty/imgui"
		"thirdparty/imgui/backends")
target_sources(imgui
	PRIVATE
		"thirdparty/imgui/imgui.cpp"
		"thirdparty/imgui/imgui_draw.cpp"
		"thirdparty/imgui/imgui_tables.cpp"
		"thirdparty/imgui/imgui_widgets.cpp"
		"thirdparty/imgui/backends/imgui_impl_glfw.cpp"
		"thirdparty/imgui/backends/imgui_impl_opengl2.cpp")
target_link_libraries(imgui PRIVATE glfw)

target_link_libraries(testbed PRIVATE imgui)
