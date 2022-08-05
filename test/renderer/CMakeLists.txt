function(add_renderer_executable GPU_BACKEND)
	set(PROJ renderer_${GPU_BACKEND})

	add_executable(${PROJ})
	set_target_properties(${PROJ} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GPU_BACKEND}/")

	target_sources(${PROJ}
		PRIVATE
			"src/common.h"
			"src/composite_pass.h"
			"src/gbuffer_pass.h"
			"src/raytrace_pass.h"
			"src/rt_resolve_pass.h"
			"src/scene.h"
			"src/shader_types.h"

			"src/main.cpp")
	target_link_libraries(${PROJ} PRIVATE lotus_renderer_${GPU_BACKEND})
endfunction()

add_renderer_executable(vulkan)
add_renderer_executable(directx12)
