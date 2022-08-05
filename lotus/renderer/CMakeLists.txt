function(add_lotus_renderer_target BACKEND)
	set(PROJ lotus_renderer_${BACKEND})

	add_library(${PROJ} STATIC)
	configure_lotus_module(${PROJ})

	target_include_directories(${PROJ} PUBLIC "include/")
	target_sources(${PROJ}
		PUBLIC
			"include/lotus/renderer/asset_manager.h"
			"include/lotus/renderer/caching.h"
			"include/lotus/renderer/common.h"
			"include/lotus/renderer/context.h"
			"include/lotus/renderer/g_buffer.h"
			"include/lotus/renderer/gltf_loader.h"
			"include/lotus/renderer/resources.h"
			"include/lotus/renderer/shader_types.h"
			"include/lotus/renderer/shader_types_impl.h"
			"include/lotus/renderer/shader_types_include_wrapper.h"
			"include/lotus/renderer/surface.h"
		PRIVATE
			"src/renderer/asset_manager.cpp"
			"src/renderer/caching.cpp"
			"src/renderer/context.cpp"
			"src/renderer/g_buffer.cpp"
			"src/renderer/gltf_loader.cpp"
			"src/renderer/resources.cpp")
	target_link_libraries(${PROJ} PUBLIC lotus_gpu_${BACKEND} tinygltf)
endfunction()

if(${LOTUS_GPU_HAS_DIRECTX12})
	add_lotus_renderer_target(directx12)
endif()
if(${LOTUS_GPU_HAS_VULKAN})
	add_lotus_renderer_target(vulkan)
endif()
