add_executable(renderer)

set(LOTUS_RENDERER_BACKEND vulkan)

target_sources(renderer
	PRIVATE
		"common.h"
		"composite_pass.h"
		"gbuffer_pass.h"
		"scene.h"

		"main.cpp")
add_shader("shaders/gbuffer.hlsl" vs main_vs 6_0 renderer ${LOTUS_RENDERER_BACKEND})
add_shader("shaders/gbuffer.hlsl" ps main_ps 6_0 renderer ${LOTUS_RENDERER_BACKEND})
add_shader("shaders/composite.hlsl" vs main_vs 6_0 renderer ${LOTUS_RENDERER_BACKEND})
add_shader("shaders/composite.hlsl" ps main_ps 6_0 renderer ${LOTUS_RENDERER_BACKEND})
target_link_libraries(renderer PRIVATE lotus_graphics_${LOTUS_RENDERER_BACKEND} tinygltf)
set_target_properties(renderer PROPERTIES
	INTERPROCEDURAL_OPTIMIZATION ON)
