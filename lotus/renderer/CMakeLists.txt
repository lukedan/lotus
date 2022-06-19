function(add_lotus_renderer_target BACKEND)
	set(PROJ lotus_renderer_${BACKEND})

	add_library(${PROJ} STATIC)
	configure_lotus_module(${PROJ})

	target_include_directories(${PROJ} PUBLIC "include/")
	target_sources(${PROJ}
		PUBLIC
			"include/lotus/renderer/asset_manager.h"
			"include/lotus/renderer/common.h"
			"include/lotus/renderer/context.h"
			"include/lotus/renderer/g_buffer.h"
			"include/lotus/renderer/gltf_loader.h"
			"include/lotus/renderer/shader_types.h"
			"include/lotus/renderer/surface.h"
		PRIVATE
			"src/renderer/asset_manager.cpp"
			"src/renderer/context.cpp"
			"src/renderer/g_buffer.cpp"
			"src/renderer/gltf_loader.cpp")
	target_link_libraries(${PROJ} PUBLIC lotus_graphics_${BACKEND} tinygltf)
endfunction()

if(${LOTUS_GRAPHICS_HAS_DIRECTX12})
	add_lotus_renderer_target(directx12)
endif()
if(${LOTUS_GRAPHICS_HAS_VULKAN})
	add_lotus_renderer_target(vulkan)
endif()