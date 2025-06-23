find_package(nlohmann_json CONFIG REQUIRED)

function(add_shadertoy_executable GPU_BACKEND)
	set(PROJ shadertoy_${GPU_BACKEND})
	add_executable(${PROJ})
	configure_lotus_module(${PROJ})
	set_target_properties(${PROJ} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GPU_BACKEND}/")

	target_sources(${PROJ}
		PRIVATE
			"src/common.h"
			"src/main.cpp"
			"src/pass.cpp"
			"src/pass.h"
			"src/project.cpp"
			"src/project.h")
	target_link_libraries(${PROJ} PRIVATE
		lotus_renderer_${GPU_BACKEND}
		lotus_helpers
		nlohmann_json::nlohmann_json)
	target_include_directories(${PROJ} PRIVATE "../../thirdparty/imgui")
endfunction()

foreach(GPU_BACKEND ${LOTUS_GPU_ALL_AVAILABLE_BACKENDS})
	add_shadertoy_executable(${GPU_BACKEND})
endforeach()
