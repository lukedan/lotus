set(LOTUS_GRAPHICS_USE_DIRECTX12 YES CACHE STRING "Whether to generate target for the DirectX 12 backend.")

set(LOTUS_GRAPHICS_BACKEND_INCLUDE_NAME "LOTUS_GRAPHICS_BACKEND_INCLUDE")
set(LOTUS_GRAPHICS_COMMON_HEADERS
	"include/lotus/graphics/commands.h"
	"include/lotus/graphics/common.h"
	"include/lotus/graphics/context.h"
	"include/lotus/graphics/descriptors.h"
	"include/lotus/graphics/device.h"
	"include/lotus/graphics/frame_buffer.h"
	"include/lotus/graphics/pass.h"
	"include/lotus/graphics/pipeline.h"
	"include/lotus/graphics/resources.h"
	"include/lotus/graphics/synchronization.h")

# DirectX 12
if(WIN32 AND ${LOTUS_GRAPHICS_USE_DIRECTX12})
	add_library(lotus_graphics_directx12 STATIC)
	set_target_compile_options(lotus_graphics_directx12)

	target_include_directories(lotus_graphics_directx12 PUBLIC "include/")
	target_sources(lotus_graphics_directx12
		PUBLIC
			${LOTUS_GRAPHICS_COMMON_HEADERS}

			"include/lotus/graphics/backends/directx12/commands.h"
			"include/lotus/graphics/backends/directx12/context.h"
			"include/lotus/graphics/backends/directx12/descriptors.h"
			"include/lotus/graphics/backends/directx12/details.h"
			"include/lotus/graphics/backends/directx12/device.h"
			"include/lotus/graphics/backends/directx12/frame_buffer.h"
			"include/lotus/graphics/backends/directx12/pass.h"
			"include/lotus/graphics/backends/directx12/pipeline.h"
			"include/lotus/graphics/backends/directx12/resources.h"
			"include/lotus/graphics/backends/directx12/synchronization.h"

			"include/lotus/graphics/backends/directx12.h"
		PRIVATE
			"src/graphics/backends/directx12/commands.cpp"
			"src/graphics/backends/directx12/context.cpp"
			"src/graphics/backends/directx12/details.cpp"
			"src/graphics/backends/directx12/device.cpp"
			"src/graphics/backends/directx12/frame_buffer.cpp"
			
			"src/graphics/common.cpp")
	target_link_libraries(lotus_graphics_directx12
		PUBLIC
			lotus_core lotus_system d3d12 dxgi dxguid)
	target_compile_definitions(lotus_graphics_directx12
		PUBLIC
			"${LOTUS_GRAPHICS_BACKEND_INCLUDE_NAME}=<lotus/graphics/backends/directx12.h>")
endif()
