add_executable(renderer)

target_sources(renderer
	PRIVATE
		"main.cpp")
target_link_libraries(renderer PRIVATE lotus)
set_target_properties(renderer PROPERTIES
	INTERPROCEDURAL_OPTIMIZATION ON)
