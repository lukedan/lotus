function(add_path_tracer_executable GPU_BACKEND)
	if(NOT TARGET lotus_renderer_${GPU_BACKEND})
		return()
	endif()

	set(PROJ path_tracer_${GPU_BACKEND})

	add_executable(${PROJ})
	set_target_properties(${PROJ} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GPU_BACKEND}/")

	target_sources(${PROJ} PRIVATE "src/main.cpp")
	target_link_libraries(${PROJ} PRIVATE lotus_renderer_${GPU_BACKEND})
	target_include_directories(${PROJ} PRIVATE "../common/include")
	target_include_directories(${PROJ} PRIVATE "../../common/include")
endfunction()

add_path_tracer_executable(vulkan)
add_path_tracer_executable(directx12)
