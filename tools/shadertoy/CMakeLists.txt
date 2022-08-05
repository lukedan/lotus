function(add_shadertoy_executable GPU_BACKEND)
	set(PROJ shadertoy_${GPU_BACKEND})
	add_executable(${PROJ})
	configure_lotus_module(${PROJ})
	set_target_properties(${PROJ} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GPU_BACKEND}/")

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
		lotus_renderer_${GPU_BACKEND}
		nlohmann_json::nlohmann_json)
endfunction()

add_shadertoy_executable(directx12)
add_shadertoy_executable(vulkan)
