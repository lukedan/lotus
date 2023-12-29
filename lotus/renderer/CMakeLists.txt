find_package(FBX)
find_package(assimp CONFIG)

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
			"include/lotus/renderer/context/execution/batch_context.h"
			"include/lotus/renderer/context/execution/caching.h"
			"include/lotus/renderer/context/execution/common.h"
			"include/lotus/renderer/context/execution/descriptors.h"
			"include/lotus/renderer/context/execution/queue_context.h"
			"include/lotus/renderer/context/execution/queue_pseudo_context.h"

			"include/lotus/renderer/context/asset_manager.h"
			"include/lotus/renderer/context/assets.h"
			"include/lotus/renderer/context/commands.h"
			"include/lotus/renderer/context/constant_uploader.h"
			"include/lotus/renderer/context/context.h"
			"include/lotus/renderer/context/misc.h"
			"include/lotus/renderer/context/resources.h"
			"include/lotus/renderer/context/resource_bindings.h"

			"include/lotus/renderer/loaders/assimp_loader.h"
			"include/lotus/renderer/loaders/dds_loader.h"
			"include/lotus/renderer/loaders/fbx_loader.h"
			"include/lotus/renderer/loaders/gltf_loader.h"

			"include/lotus/renderer/common.h"
			"include/lotus/renderer/dear_imgui.h"
			"include/lotus/renderer/debug_drawing.h"
			"include/lotus/renderer/g_buffer.h"
			"include/lotus/renderer/generic_pbr_material.h"
			"include/lotus/renderer/mipmap.h"
			"include/lotus/renderer/shader_types.h"
			"include/lotus/renderer/shader_types_impl.h"
			"include/lotus/renderer/shader_types_include_wrapper.h"
		PRIVATE
			"src/renderer/context/execution/batch_context.cpp"
			"src/renderer/context/execution/caching.cpp"
			"src/renderer/context/execution/descriptors.cpp"
			"src/renderer/context/execution/queue_context.cpp"
			"src/renderer/context/execution/queue_pseudo_context.cpp"

			"src/renderer/context/asset_manager.cpp"
			"src/renderer/context/constant_uploader.cpp"
			"src/renderer/context/context.cpp"
			"src/renderer/context/resources.cpp"
			"src/renderer/context/resource_bindings.cpp"

			"src/renderer/loaders/assimp_loader.cpp"
			"src/renderer/loaders/dds_loader.cpp"
			"src/renderer/loaders/fbx_loader.cpp"
			"src/renderer/loaders/gltf_loader.cpp"

			"src/renderer/g_buffer.cpp"
			"src/renderer/generic_pbr_material.cpp"
			"src/renderer/mipmap.cpp")
	target_link_libraries(${PROJ} PUBLIC lotus_gpu_${BACKEND} tinygltf)
	if(FBX_FOUND)
		target_link_libraries(${PROJ} PUBLIC FBX::FBX)
		target_compile_definitions(${PROJ} PUBLIC LOTUS_RENDERER_HAS_FBX)
	endif()
	if(assimp_FOUND)
		target_link_libraries(${PROJ} PUBLIC assimp::assimp)
		target_compile_definitions(${PROJ} PUBLIC LOTUS_RENDERER_HAS_ASSIMP)
	endif()
endfunction()

if(${LOTUS_GPU_HAS_DIRECTX12})
	add_lotus_renderer_target(directx12)
endif()
if(${LOTUS_GPU_HAS_VULKAN})
	add_lotus_renderer_target(vulkan)
endif()
