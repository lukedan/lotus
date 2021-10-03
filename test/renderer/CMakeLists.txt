add_executable(renderer)

target_sources(renderer
	PRIVATE
		"common.h"
		"composite_pass.h"
		"gbuffer_pass.h"
		"scene.h"

		"main.cpp")
target_link_libraries(renderer PRIVATE lotus_graphics_directx12 tinygltf)
set_target_properties(renderer PROPERTIES
	INTERPROCEDURAL_OPTIMIZATION ON)
