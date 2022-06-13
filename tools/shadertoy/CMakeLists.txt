function(add_shadertoy_executable GRAPHICS_BACKEND)
	set(PROJ shadertoy_${GRAPHICS_BACKEND})
	add_executable(${PROJ})
	configure_lotus_module(${PROJ})

	find_package(nlohmann_json CONFIG REQUIRED)

	target_sources(${PROJ}
		PRIVATE
			"src/common.h"
			"src/main.cpp"
			"src/pass.cpp"
			"src/pass.h"
			"src/project.cpp"
			"src/project.h"
			"src/stb.cpp")
	target_include_directories(${PROJ}
		PRIVATE "thirdparty/stb/")
	target_link_libraries(${PROJ} PRIVATE
		lotus_renderer_${GRAPHICS_BACKEND}
		nlohmann_json::nlohmann_json)

	add_shader("shaders/vertex.hlsl" vs main_vs 6_0 ${PROJ} ${GRAPHICS_BACKEND})
	add_shader("shaders/blit.hlsl" ps main_ps 6_0 ${PROJ} ${GRAPHICS_BACKEND})
endfunction()

add_shadertoy_executable(directx12)
add_shadertoy_executable(vulkan)
