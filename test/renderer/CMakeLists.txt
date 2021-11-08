function(add_renderer_executable GRAPHICS_BACKEND)
	set(PROJ renderer_${GRAPHICS_BACKEND})

	add_executable(${PROJ})

	target_sources(${PROJ}
		PRIVATE
			"common.h"
			"composite_pass.h"
			"gbuffer_pass.h"
			"scene.h"

			"main.cpp")
	add_shader("shaders/gbuffer.hlsl" vs main_vs 6_0 ${PROJ} ${GRAPHICS_BACKEND})
	add_shader("shaders/gbuffer.hlsl" ps main_ps 6_0 ${PROJ} ${GRAPHICS_BACKEND})
	add_shader("shaders/composite.hlsl" vs main_vs 6_0 ${PROJ} ${GRAPHICS_BACKEND})
	add_shader("shaders/composite.hlsl" ps main_ps 6_0 ${PROJ} ${GRAPHICS_BACKEND})
	target_link_libraries(${PROJ} PRIVATE lotus_graphics_${GRAPHICS_BACKEND} tinygltf)
endfunction()

add_renderer_executable(vulkan)
add_renderer_executable(directx12)
