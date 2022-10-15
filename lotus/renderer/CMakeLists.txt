find_package(FBX)

set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_HEADER_ONLY OFF CACHE INTERNAL "" FORCE)
add_subdirectory("thirdparty/tinygltf/")

function(add_lotus_renderer_target BACKEND)
	set(PROJ lotus_renderer_${BACKEND})

	add_library(${PROJ} STATIC)
	configure_lotus_module(${PROJ})

	target_include_directories(${PROJ} PUBLIC "include/")
	target_sources(${PROJ}
		PUBLIC
			"include/lotus/renderer/asset_manager.h"
			"include/lotus/renderer/assets.h"
			"include/lotus/renderer/caching.h"
			"include/lotus/renderer/common.h"
			"include/lotus/renderer/context.h"
			"include/lotus/renderer/execution.h"
			"include/lotus/renderer/g_buffer.h"
			"include/lotus/renderer/generic_pbr_material.h"
			"include/lotus/renderer/mipmap.h"
			"include/lotus/renderer/resource_bindings.h"
			"include/lotus/renderer/resources.h"
			"include/lotus/renderer/shader_types.h"
			"include/lotus/renderer/shader_types_impl.h"
			"include/lotus/renderer/shader_types_include_wrapper.h"

			"include/lotus/renderer/loaders/dds_loader.h"
			"include/lotus/renderer/loaders/fbx_loader.h"
			"include/lotus/renderer/loaders/gltf_loader.h"
		PRIVATE
			"src/renderer/asset_manager.cpp"
			"src/renderer/caching.cpp"
			"src/renderer/context.cpp"
			"src/renderer/execution.cpp"
			"src/renderer/g_buffer.cpp"
			"src/renderer/generic_pbr_material.cpp"
			"src/renderer/mipmap.cpp"
			"src/renderer/resource_bindings.cpp"
			"src/renderer/resources.cpp"

			"src/renderer/loaders/dds_loader.cpp"
			"src/renderer/loaders/fbx_loader.cpp"
			"src/renderer/loaders/gltf_loader.cpp")
	target_link_libraries(${PROJ} PUBLIC lotus_gpu_${BACKEND} tinygltf)
	if(FBX_FOUND)
		target_link_libraries(${PROJ} PUBLIC FBX::FBX)
		target_compile_definitions(${PROJ} PUBLIC LOTUS_RENDERER_HAS_FBX)
	endif()
endfunction()

if(${LOTUS_GPU_HAS_DIRECTX12})
	add_lotus_renderer_target(directx12)
endif()
if(${LOTUS_GPU_HAS_VULKAN})
	add_lotus_renderer_target(vulkan)
endif()
